#include "document.hpp"
#include "decimal/decimal.hpp"
#include "batch.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <charconv>
#include <set>

namespace kumwe::engine::document {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
const value null;
const value& member(const value& v, std::string_view k) { const auto* p = v.find(k); return p ? *p : null; }
bool scalar(const value& v) { return v.is<std::nullptr_t>() || v.is<bool>() || v.is<std::int64_t>() || v.is<std::string>(); }
bool tagged(const value& v) { return member(v, "type") == value("normalized-value"); }
bool exact(const value& v) { return member(v, "type") == value("exact-decimal"); }
bool fields(const value& v, std::initializer_list<std::string_view> names) {
    if (!v.is<object>() || v.as<object>().size() != names.size()) return false;
    return std::all_of(names.begin(), names.end(), [&](std::string_view name) { return v.find(name) != nullptr; });
}
bool instance_valid(const value& v) {
    const auto* identity = v.find("instance");
    return identity == nullptr || (identity->is<std::int64_t>() && identity->as<std::int64_t>() > 0);
}
bool domain_fields(const value& v, std::initializer_list<std::string_view> names) {
    if (!v.is<object>() || !instance_valid(v)) return false;
    if (v.as<object>().size() != names.size() + (v.find("instance") == nullptr ? 0 : 1)) return false;
    return std::all_of(names.begin(), names.end(), [&](std::string_view name) { return v.find(name) != nullptr; });
}
bool strings(const value& v, std::initializer_list<std::string_view> names) {
    if (!fields(v, names)) return false;
    return std::all_of(names.begin(), names.end(), [&](std::string_view name) { return v.at(name).is<std::string>(); });
}
bool canonical_decimal(const value& v) {
    if (!v.is<std::string>()) return false;
    const auto& text = v.as<std::string>(); const auto dot = text.find('.');
    const auto scale = dot == std::string::npos ? 0 : text.size() - dot - 1;
    if (scale > 65) return false;
    try { return decimal::value::parse(text, 65, static_cast<unsigned>(scale)).literal() == text; }
    catch (const decimal::invalid_decimal&) { return false; }
}
bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool identifier(const std::string& s, std::size_t maximum, std::string_view punctuation) {
    return !s.empty() && s.size() <= maximum && (alpha(s.front()) || digit(s.front()))
        && std::all_of(s.begin(), s.end(), [&](char c) { return alpha(c) || digit(c) || punctuation.find(c) != std::string_view::npos; });
}
bool base64(const std::string& s, std::size_t minimum, std::size_t maximum) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (s.empty() || s.size() % 4 != 0) return false;
    const std::size_t padding = s.back() == '=' ? (s[s.size() - 2] == '=' ? 2 : 1) : 0;
    const auto size = s.size() / 4 * 3 - padding;
    if (size < minimum || size > maximum) return false;
    for (std::size_t i = 0; i < s.size() - padding; ++i) if (alphabet.find(s[i]) == std::string_view::npos) return false;
    return padding == 0 || (alphabet.find(s[s.size() - padding - 1]) & (padding == 2 ? 15U : 3U)) == 0;
}
bool timestamp(const value& v, bool utc) {
    if (!v.is<std::string>()) return false;
    const auto& s = v.as<std::string>();
    if (s.size() < 27 || s.size() > 64) return false;
    const std::size_t year_start = s.front() == '-' ? 1 : 0;
    const auto end = s.find('-', year_start);
    if (end == std::string::npos || end - year_start < 4 || end - year_start > 19
        || (end - year_start > 4 && s[year_start] == '0') || s.size() < end + 23) return false;
    unsigned year_mod = 0;
    for (std::size_t i = year_start; i < end; ++i) { if (!digit(s[i])) return false; year_mod = (year_mod * 10 + static_cast<unsigned>(s[i] - '0')) % 400; }
    for (std::size_t i = 0; i < 22; ++i) {
        const char delimiter = i == 0 || i == 3 ? '-' : i == 6 ? 'T' : i == 9 || i == 12 ? ':' : i == 15 ? '.' : '\0';
        if (delimiter ? s[end + i] != delimiter : !digit(s[end + i])) return false;
    }
    const auto two = [&](std::size_t offset) { return static_cast<unsigned>((s[end + offset] - '0') * 10 + s[end + offset + 1] - '0'); };
    const auto month = two(1), day = two(4);
    if (month < 1 || month > 12 || day < 1 || two(7) > 23 || two(10) > 59 || two(13) > 59) return false;
    const unsigned month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const auto maximum = month_days[month - 1] + (month == 2 && year_mod % 4 == 0 && (year_mod % 100 != 0 || year_mod == 0) ? 1U : 0U);
    if (day > maximum) return false;
    if (utc) return s.size() == end + 23 && s[end + 22] == 'Z';
    if (s.size() < end + 28 || (s[end + 22] != '+' && s[end + 22] != '-') || s[s.size() - 3] != ':') return false;
    for (std::size_t i = end + 23; i < s.size() - 3; ++i) if (!digit(s[i])) return false;
    return digit(s[s.size() - 2]) && digit(s.back()) && s[s.size() - 2] <= '5';
}

bool canonical_key(const value& key) {
    if (key.is<std::int64_t>()) return true;
    if (!key.is<std::string>()) return false;
    const auto& s = key.as<std::string>(); std::int64_t integer = 0;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), integer);
    // PHP has already converted canonical integer spellings into integer keys.
    return parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size() || std::to_string(integer) != s;
}
std::string key_string(const value& key) { return key.is<std::string>() ? key.as<std::string>() : std::to_string(key.as<std::int64_t>()); }
bool admit(const value& v, unsigned depth, unsigned& nodes) {
    if (++nodes > 4096 || depth > 8) return false;
    if (scalar(v)) return true;
    if (exact(v)) return domain_fields(v, {"type", "value"}) && canonical_decimal(v.at("value"));
    if (tagged(v)) {
        if (!domain_fields(v, {"type", "version", "kind", "value"}) || member(v, "version") != value(std::int64_t{1})
            || !member(v, "kind").is<std::string>()) return false;
        const auto& kind = v.at("kind").as<std::string>(); const auto& payload = v.at("value");
        if (kind == "datetime") return timestamp(payload, false);
        if (kind == "zoned-datetime") return strings(payload, {"instant", "timezone"}) && timestamp(payload.at("instant"), true)
            && !payload.at("timezone").as<std::string>().empty() && payload.at("timezone").as<std::string>().size() <= 255;
        if (kind == "money") {
            if (!strings(payload, {"amount", "currency"}) || !canonical_decimal(payload.at("amount"))) return false;
            const auto& currency = payload.at("currency").as<std::string>();
            return currency.size() == 3 && std::all_of(currency.begin(), currency.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
        }
        if (kind == "quantity") return strings(payload, {"amount", "unit"}) && canonical_decimal(payload.at("amount"))
            && identifier(payload.at("unit").as<std::string>(), 63, "._/-");
        if (kind == "encrypted") return strings(payload, {"ciphertext", "nonce", "key_id", "algorithm"})
            && payload.at("algorithm") == value("xchacha20poly1305-ietf") && base64(payload.at("ciphertext").as<std::string>(), 1, 1048576)
            && base64(payload.at("nonce").as<std::string>(), 24, 24)
            && identifier(payload.at("key_id").as<std::string>(), 127, "._:-");
        if (kind != "array" || v.find("instance") != nullptr || !fields(payload, {"entries"}) || !payload.at("entries").is<list>()) return false;
        std::set<std::string> keys;
        for (const auto& entry : payload.at("entries").as<list>()) {
            if (!fields(entry, {"key", "value"}) || !canonical_key(entry.at("key"))
                || !keys.emplace(key_string(entry.at("key"))).second || !admit(entry.at("value"), depth + 1, nodes)) return false;
        }
        return true;
    }
    if (v.is<list>()) {
        for (const auto& child : v.as<list>()) if (!admit(child, depth + 1, nodes)) return false;
        return true;
    }
    if (v.is<object>()) {
        for (const auto& [key, child] : v.as<object>()) { (void)key; if (!admit(child, depth + 1, nodes)) return false; }
        return true;
    }
    return false;
}
value php_array(object out) {
    bool sequential = true; std::size_t index = 0;
    for (const auto& [key, child] : out) { (void)child; if (key != std::to_string(index++)) sequential = false; }
    if (!sequential) return value(std::move(out));
    list values; for (auto& [key, child] : out) { (void)key; values.emplace_back(std::move(child)); }
    return value(std::move(values));
}
value canonical(const value& v) {
    if (exact(v)) return v.at("value");
    if (tagged(v)) {
        if (v.at("kind") != value("array")) return v.at("value");
        const auto& entries = v.at("value").at("entries").as<list>();
        bool sequential = true;
        for (std::size_t i = 0; i < entries.size(); ++i) if (entries[i].at("key") != value(static_cast<std::int64_t>(i))) sequential = false;
        if (sequential) { list out; for (const auto& entry : entries) out.emplace_back(canonical(entry.at("value"))); return value(std::move(out)); }
        object out; for (const auto& entry : entries) out.emplace(key_string(entry.at("key")), canonical(entry.at("value")));
        return php_array(std::move(out));
    }
    if (v.is<list>()) { list out; for (const auto& child : v.as<list>()) out.emplace_back(canonical(child)); return value(std::move(out)); }
    if (v.is<object>()) { object out; for (const auto& [key, child] : v.as<object>()) out.emplace(key, canonical(child)); return php_array(std::move(out)); }
    return v;
}
list entries(const value& v) {
    if (tagged(v) && v.at("kind") == value("array")) return v.at("value").at("entries").as<list>();
    list out;
    if (v.is<list>()) for (std::size_t i = 0; i < v.as<list>().size(); ++i) out.emplace_back(object{{"key", value(static_cast<std::int64_t>(i))}, {"value", v.as<list>()[i]}});
    if (v.is<object>()) for (const auto& [key, child] : v.as<object>()) {
        std::int64_t integer = 0; const auto parsed = std::from_chars(key.data(), key.data() + key.size(), integer);
        value php_key = parsed.ec == std::errc{} && parsed.ptr == key.data() + key.size() && std::to_string(integer) == key ? value(integer) : value(key);
        out.emplace_back(object{{"key", std::move(php_key)}, {"value", child}});
    }
    return out;
}
bool same_domain_value(const value& a, const value& b) {
    return member(a, "type") == member(b, "type") && member(a, "kind") == member(b, "kind")
        && member(a, "value") == member(b, "value");
}
bool strict(const value& a, const value& b) {
    if (normalized_domain_object(a) || normalized_domain_object(b)) {
        if (!normalized_domain_object(a) || !normalized_domain_object(b)) return false;
        const auto* left = a.find("instance"); const auto* right = b.find("instance");
        const bool same_payload = same_domain_value(a, b);
        if (!same_payload) {
            if (left != nullptr && right != nullptr && *left == *right) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            return false;
        }
        if (left == nullptr || right == nullptr) throw refusal(KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY);
        return *left == *right;
    }
    if (scalar(a) || scalar(b)) return a == b;
    const auto left = entries(a), right = entries(b);
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i)
        if (left[i].at("key") != right[i].at("key") || !strict(left[i].at("value"), right[i].at("value"))) return false;
    return true;
}
void collect_instances(const value& v, std::map<std::int64_t, const value*>& instances) {
    if (normalized_domain_object(v)) {
        if (!normalized_value_valid(v)) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        if (const auto* identity = v.find("instance")) {
            const auto [position, inserted] = instances.emplace(identity->as<std::int64_t>(), &v);
            if (!inserted && !same_domain_value(v, *position->second)) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        }
        return;
    }
    if (v.is<list>()) for (const auto& child : v.as<list>()) collect_instances(child, instances);
    if (v.is<object>()) for (const auto& [key, child] : v.as<object>()) { (void)key; collect_instances(child, instances); }
}
void require(const value& v) { if (!normalized_value_valid(v)) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT); }
}
bool normalized_value_valid(const value& v) { unsigned nodes = 0; return admit(v, 0, nodes); }
void validate_normalized_instances(const std::vector<const value*>& inputs) {
    std::map<std::int64_t, const value*> instances;
    for (const auto* input : inputs) { if (input != nullptr) collect_instances(*input, instances); }
}
bool normalized_domain_object(const value& v) { return exact(v) || (tagged(v) && member(v, "kind") != value("array")); }
bool normalized_exact_decimal(const value& v) { return exact(v); }
bool normalized_strict_equal(const value& current, const value& submitted) { require(current); require(submitted); return strict(current, submitted); }
value output_value(const value& v) { require(v); return canonical(v); }
value expression_value(const value& v) {
    require(v);
    if (exact(v)) return v.at("value");
    if (tagged(v)) {
        if (v.at("kind") == value("datetime")) return v.at("value");
        if (v.at("kind") == value("zoned-datetime")) return v.at("value").at("instant");
    }
    return scalar(v) ? v : value();
}
}
