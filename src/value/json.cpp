#include "json.hpp"
#include "batch.hpp"
#include <kumwe/engine/engine.h>
#include <charconv>
#include <limits>

namespace kumwe::engine::json {
namespace {
[[noreturn]] void invalid() { throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT); }
[[noreturn]] void exhausted() { throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT); }
void utf8(std::string& output, std::uint32_t cp) {
    if (cp < 0x80) output.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        output.push_back(static_cast<char>(0xc0U | (cp >> 6U)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    } else if (cp < 0x10000) {
        output.push_back(static_cast<char>(0xe0U | (cp >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 63U)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    } else {
        output.push_back(static_cast<char>(0xf0U | (cp >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 63U)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 63U)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    }
}
class reader final {
    std::string_view bytes;
    std::size_t remaining, depth_limit;
    char take() { if (bytes.empty()) invalid(); const char c = bytes.front(); bytes.remove_prefix(1); return c; }
    bool consume(char c) { if (!bytes.empty() && bytes.front() == c) { bytes.remove_prefix(1); return true; } return false; }
    std::uint32_t hex4() {
        std::uint32_t cp = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const char c = take();
            if (c >= '0' && c <= '9') cp = cp * 16 + static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') cp = cp * 16 + static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp = cp * 16 + static_cast<unsigned>(c - 'A' + 10);
            else invalid();
        }
        return cp;
    }
    std::string string() {
        if (take() != '"') invalid();
        std::string output;
        while (true) {
            const char c = take();
            if (c == '"') break;
            if (static_cast<unsigned char>(c) < 32) invalid();
            if (c != '\\') { output.push_back(c); continue; }
            switch (take()) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                auto cp = hex4();
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    if (take() != '\\' || take() != 'u') invalid();
                    const auto low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) invalid();
                    cp = 0x10000U + ((cp - 0xd800U) << 10U) + low - 0xdc00U;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) invalid();
                utf8(output, cp); break;
            }
            default: invalid();
            }
        }
        if (!valid_utf8(output)) invalid();
        return output;
    }
    value numeric() {
        const auto initial = bytes;
        consume('-');
        if (!consume('0')) {
            if (bytes.empty() || bytes.front() < '1' || bytes.front() > '9') invalid();
            do { bytes.remove_prefix(1); } while (!bytes.empty() && bytes.front() >= '0' && bytes.front() <= '9');
        }
        bool integral = true;
        if (consume('.')) {
            integral = false;
            if (bytes.empty() || bytes.front() < '0' || bytes.front() > '9') invalid();
            do { bytes.remove_prefix(1); } while (!bytes.empty() && bytes.front() >= '0' && bytes.front() <= '9');
        }
        if (consume('e') || consume('E')) {
            integral = false;
            if (!consume('+')) consume('-');
            if (bytes.empty() || bytes.front() < '0' || bytes.front() > '9') invalid();
            do { bytes.remove_prefix(1); } while (!bytes.empty() && bytes.front() >= '0' && bytes.front() <= '9');
        }
        const auto spelling = initial.substr(0, initial.size() - bytes.size());
        if (integral) {
            std::int64_t integer = 0;
            const auto result = std::from_chars(spelling.data(), spelling.data() + spelling.size(), integer);
            if (result.ec == std::errc{} && result.ptr == spelling.data() + spelling.size()) return value(integer);
        }
        return value(number{std::string(spelling)});
    }
public:
    reader(std::string_view input, std::size_t nodes, std::size_t depth) : bytes(input), remaining(nodes), depth_limit(depth) {}
    void spaces() { while (!bytes.empty() && (bytes.front() == ' ' || bytes.front() == '\n' || bytes.front() == '\r' || bytes.front() == '\t')) bytes.remove_prefix(1); }
    value next(std::size_t depth = 0) {
        if (depth > depth_limit || remaining == 0) exhausted();
        --remaining; spaces(); if (bytes.empty()) invalid();
        if (bytes.front() == '"') return value(string());
        for (const auto token : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
            if (bytes.starts_with(token)) { bytes.remove_prefix(token.size()); if (token == "null") return {}; return value(token == "true"); }
        }
        if (consume('[')) {
            value::list items; spaces(); if (consume(']')) return value(std::move(items));
            while (true) { items.push_back(next(depth + 1)); spaces(); if (consume(']')) break; if (!consume(',')) invalid(); }
            return value(std::move(items));
        }
        if (consume('{')) {
            value::object items; spaces(); if (consume('}')) return value(std::move(items));
            while (true) {
                spaces(); const auto key = string(); spaces(); if (!consume(':')) invalid();
                if (!items.emplace(key, next(depth + 1)).second) invalid();
                spaces(); if (consume('}')) break; if (!consume(',')) invalid();
            }
            return value(std::move(items));
        }
        return numeric();
    }
    bool complete() { spaces(); return bytes.empty(); }
};
template<bool Materialize>
class writer final {
    std::string bytes;
    std::size_t limit;
    std::size_t counted = 0;
    void append(std::string_view token) {
        if (token.size() > limit - size()) exhausted();
        if constexpr (Materialize) bytes += token;
        else counted += token.size();
    }
    void string(std::string_view input) {
        if (!valid_utf8(input)) invalid();
        append("\"");
        constexpr char hex[] = "0123456789abcdef";
        std::size_t start = 0;
        for (std::size_t i = 0; i < input.size(); ++i) {
            const auto c = static_cast<unsigned char>(input[i]);
            auto escape = [&](std::string_view text, std::size_t consumed = 1) {
                append(input.substr(start, i - start));
                append(text);
                i += consumed - 1;
                start = i + 1;
            };
            switch (c) {
            case '"': escape("\\\""); break;
            case '\\': escape("\\\\"); break;
            case '\b': escape("\\b"); break;
            case '\f': escape("\\f"); break;
            case '\n': escape("\\n"); break;
            case '\r': escape("\\r"); break;
            case '\t': escape("\\t"); break;
            default:
                if (c < 32) { const char escaped[] = {'\\','u','0','0',hex[c >> 4U],hex[c & 15U]}; escape(std::string_view(escaped, 6)); }
                else if (c == 0xe2 && i + 2 < input.size() && static_cast<unsigned char>(input[i + 1]) == 0x80 &&
                         (static_cast<unsigned char>(input[i + 2]) == 0xa8 || static_cast<unsigned char>(input[i + 2]) == 0xa9)) {
                    escape(static_cast<unsigned char>(input[i + 2]) == 0xa8 ? "\\u2028" : "\\u2029", 3);
                }
            }
        }
        append(input.substr(start));
        append("\"");
    }
public:
    explicit writer(std::size_t max) : limit(max) {}
    std::size_t size() const noexcept {
        if constexpr (Materialize) return bytes.size();
        else return counted;
    }
    void add(const value& item) {
        if (item.is<std::nullptr_t>()) append("null");
        else if (item.is<bool>()) append(item.as<bool>() ? "true" : "false");
        else if (item.is<std::int64_t>()) append(std::to_string(item.as<std::int64_t>()));
        else if (item.is<std::string>()) string(item.as<std::string>());
        else if (item.is<number>()) append(item.as<number>().spelling);
        else if (item.is<value::list>()) {
            append("["); bool comma = false;
            for (const auto& entry : item.as<value::list>()) { if (comma) append(","); add(entry); comma = true; } append("]");
        } else {
            append("{"); bool comma = false;
            for (const auto& [key, entry] : item.as<value::object>()) { if (comma) append(","); string(key); append(":"); add(entry); comma = true; } append("}");
        }
    }
    std::string finish() { return std::move(bytes); }
};
}
const value* value::find(std::string_view key) const noexcept {
    if (!is<object>()) return nullptr;
    const auto& map = as<object>(); const auto found = map.find(key);
    return found == map.end() ? nullptr : &found->second;
}
const value& value::at(std::string_view key) const { const auto* found = find(key); if (!found) invalid(); return *found; }
value parse(std::string_view source, std::size_t max_bytes, std::size_t max_nodes, std::size_t max_depth) {
    if (source.size() > max_bytes) exhausted();
    reader input(source, max_nodes, max_depth);
    auto result = input.next(); if (!input.complete()) invalid(); return result;
}
std::string encode(const value& source, std::size_t max_bytes) { writer<true> output(max_bytes); output.add(source); return output.finish(); }
std::size_t encoded_size(const value& source, std::size_t max_bytes) { writer<false> output(max_bytes); output.add(source); return output.size(); }
bool valid_utf8(std::string_view source) noexcept {
    for (std::size_t i = 0; i < source.size();) {
        const auto first = static_cast<unsigned char>(source[i++]);
        if (first < 0x80) continue;
        unsigned count = 0; std::uint32_t cp = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 1; cp = first & 31U; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { count = 2; cp = first & 15U; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 3; cp = first & 7U; minimum = 0x10000; }
        else return false;
        if (count > source.size() - i) return false;
        while (count-- != 0) { const auto next = static_cast<unsigned char>(source[i++]); if ((next & 0xc0U) != 0x80U) return false; cp = (cp << 6U) | (next & 63U); }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}
}
