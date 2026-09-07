#include "computed_value.hpp"
#include "unicode_normalization.hpp"
#include "batch.hpp"
#include "decimal/decimal.hpp"
#include "vm/error.hpp"
#include <kumwe/engine/engine.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>
#include <algorithm>
#include <array>
#include <memory>
#include <set>

namespace kumwe::engine::document {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
const value null;
const value& member(const value& input, std::string_view key) {
    const auto* result = input.find(key);
    return result == nullptr ? null : *result;
}
[[noreturn]] void invalid() { throw refusal(KUMWE_ENGINE_V1_INVALID_PROGRAM); }
[[noreturn]] void unsupported() { throw refusal(KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY); }
[[noreturn]] void failed() { vm::reject("A computed value cannot be normalized for its field."); }
void charge(std::uint64_t& budget, std::size_t cost) {
    if (cost > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= cost;
}
bool digit(char c) { return c >= '0' && c <= '9'; }
bool lowercase(char c) { return c >= 'a' && c <= 'z'; }
bool identifier(std::string_view text) {
    if (text.empty() || text.size() > 63 || !lowercase(text.front())) return false;
    return std::all_of(text.begin(), text.end(), [](char c) {
        return lowercase(c) || digit(c) || c == '.' || c == '_' || c == '-';
    });
}
bool one_of(std::string_view text, std::initializer_list<std::string_view> options) {
    return std::find(options.begin(), options.end(), text) != options.end();
}
unsigned integer(const value& candidate, unsigned minimum, unsigned maximum) {
    if (!candidate.is<std::int64_t>() || candidate.as<std::int64_t>() < minimum
        || candidate.as<std::int64_t>() > maximum) invalid();
    return static_cast<unsigned>(candidate.as<std::int64_t>());
}
bool trim_byte(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0' || c == '\v'; }
std::string trim(std::string_view text) {
    while (!text.empty() && trim_byte(text.front())) text.remove_prefix(1);
    while (!text.empty() && trim_byte(text.back())) text.remove_suffix(1);
    return std::string(text);
}
std::string phone(std::string_view text) {
    using code_owner = std::unique_ptr<pcre2_code, decltype(&pcre2_code_free)>;
    static const code_owner pattern = []() {
        constexpr std::string_view expression = R"([\s().-]+)";
        int error; PCRE2_SIZE offset;
        auto* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(expression.data()), expression.size(),
            PCRE2_UTF | PCRE2_UCP, &error, &offset, nullptr);
        if (code == nullptr) throw std::bad_alloc();
        return code_owner(code, &pcre2_code_free);
    }();
    using context_owner = std::unique_ptr<pcre2_match_context, decltype(&pcre2_match_context_free)>;
    context_owner context(pcre2_match_context_create(nullptr), &pcre2_match_context_free);
    if (!context) throw std::bad_alloc();
    pcre2_set_match_limit(context.get(), 1000000);
    pcre2_set_depth_limit(context.get(), 100000);
    pcre2_set_heap_limit(context.get(), 8192);
    std::string output(text.size() + 1, '\0');
    PCRE2_SIZE size = output.size();
    const auto status = pcre2_substitute(pattern.get(), reinterpret_cast<PCRE2_SPTR>(text.data()), text.size(),
        0, PCRE2_SUBSTITUTE_GLOBAL, nullptr, context.get(), reinterpret_cast<PCRE2_SPTR>(""), 0,
        reinterpret_cast<PCRE2_UCHAR*>(output.data()), &size);
    if (status < 0) failed();
    output.resize(size);
    return output;
}
unsigned part(std::string_view text, std::size_t start, std::size_t size) {
    unsigned result = 0;
    for (std::size_t i = start; i < start + size; ++i) {
        if (!digit(text[i])) failed();
        result = result * 10 + static_cast<unsigned>(text[i] - '0');
    }
    return result;
}
void date(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') failed();
    const auto year = part(text, 0, 4), month = part(text, 5, 2), day = part(text, 8, 2);
    if (year < 1000 || month < 1 || month > 12 || day < 1) failed();
    constexpr std::array<unsigned, 12> days{31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (day > days[month - 1] + (month == 2 && leap ? 1U : 0U)) failed();
}
void clock(std::string_view text) {
    if (text.size() != 8 || text[2] != ':' || text[5] != ':'
        || part(text, 0, 2) > 23 || part(text, 3, 2) > 59 || part(text, 6, 2) > 59) failed();
}
std::string temporal(std::string_view text, std::string_view type) {
    if (type == "date") {
        date(text);
        return std::string(text) + "T00:00:00.000000+00:00";
    }
    if (type == "time") {
        if (text.size() != 8 && text.size() != 15) failed();
        clock(text.substr(0, 8));
        if (text.size() == 15) {
            if (text[8] != '.') failed();
            (void)part(text, 9, 6);
        }
        return "1970-01-01T" + std::string(text) + (text.size() == 8 ? ".000000+00:00" : "+00:00");
    }
    if (text.size() < 20 || text[10] != 'T') failed();
    date(text.substr(0, 10)); clock(text.substr(11, 8));
    std::size_t zone;
    if (text.ends_with('Z')) zone = text.size() - 1;
    else if (text.ends_with("+00:00")) zone = text.size() - 6;
    else failed();
    std::string fraction;
    if (zone != 19) {
        if (zone < 21 || zone > 26 || text[19] != '.') failed();
        (void)part(text, 20, zone - 20);
        fraction = text.substr(20, zone - 20);
    }
    fraction.append(6 - fraction.size(), '0');
    return std::string(text.substr(0, 19)) + '.' + fraction + "+00:00";
}
}
void validate_computed_definition(const value& definition) {
    const auto& declared = member(definition, "type");
    if (!declared.is<std::nullptr_t>() && !declared.is<std::string>()) invalid();
    if (!declared.is<std::nullptr_t>() && declared != value("core.computed")) unsupported();
    const auto& type = member(member(definition, "formula"), "type");
    if (!type.is<std::string>()) invalid();
    if (!one_of(type.as<std::string>(), {"boolean","integer","decimal","string","date","time","datetime"})) unsupported();
    const auto& precision = member(definition, "precision"); const auto& scale = member(definition, "scale");
    if (precision.is<std::nullptr_t>() != scale.is<std::nullptr_t>()) invalid();
    if (!precision.is<std::nullptr_t>()) {
        const auto p = integer(precision, 1, 65), s = integer(scale, 0, 30);
        if (s > p) invalid();
    }
    const auto& length = member(definition, "length");
    if (!length.is<std::nullptr_t>()) (void)integer(length, 1, 1000000);
    const auto& normalizers = member(definition, "normalizers");
    if (normalizers.is<std::nullptr_t>()) return;
    if (!normalizers.is<list>() || normalizers.as<list>().size() > 32) invalid();
    std::set<std::string> seen;
    for (const auto& normalizer : normalizers.as<list>()) {
        if (!normalizer.is<std::string>() || !identifier(normalizer.as<std::string>())
            || !seen.emplace(normalizer.as<std::string>()).second) invalid();
    }
}
value normalize_computed_value(value output, const value& definition, std::uint64_t& budget, std::size_t output_limit) {
    if (output.is<std::nullptr_t>()) return output;
    const auto& normalizers = member(definition, "normalizers");
    if (normalizers.is<list>()) for (const auto& normalizer : normalizers.as<list>()) {
        charge(budget, 1);
        if (normalizer == value("decimal_scale")) {
            if (!output.is<std::string>() && !output.is<std::int64_t>()) failed();
            continue;
        }
        if (!output.is<std::string>()) failed();
        charge(budget, output.as<std::string>().size());
        if (normalizer == value("trim") || normalizer == value("url")) output = value(trim(output.as<std::string>()));
        else if (normalizer == value("phone")) output = value(phone(trim(output.as<std::string>())));
        else if (normalizer == value("email")) output = value(normalize_unicode(trim(output.as<std::string>()), "lowercase", budget, output_limit));
        else if (normalizer == value("lowercase") || normalizer == value("uppercase") || normalizer == value("unicode_nfc"))
            output = value(normalize_unicode(output.as<std::string>(), normalizer.as<std::string>(), budget, output_limit));
        else failed();
    }
    const auto& type = definition.at("formula").at("type").as<std::string>();
    if (type == "boolean") {
        if (!output.is<bool>()) failed();
    } else if (type == "integer") {
        if (!output.is<std::int64_t>() || output.as<std::int64_t>() < -2147483648LL || output.as<std::int64_t>() > 2147483647LL) failed();
    } else if (type == "decimal") {
        if (!output.is<std::string>() && !output.is<std::int64_t>()) failed();
        if (member(definition, "precision").is<std::nullptr_t>() || member(definition, "scale").is<std::nullptr_t>()) failed();
        const auto source = output.is<std::string>() ? output.as<std::string>() : std::to_string(output.as<std::int64_t>());
        charge(budget, source.size());
        try {
            const auto number = decimal::value::parse(source, integer(definition.at("precision"), 1, 65),
                integer(definition.at("scale"), 0, 30));
            output = value(object{{"type", value("exact-decimal")}, {"value", value(number.literal())}});
        } catch (const decimal::invalid_decimal&) { failed(); }
    } else {
        if (!output.is<std::string>()) failed();
        const auto& source = output.as<std::string>();
        charge(budget, source.size());
        if (type == "string") {
            const auto& length = member(definition, "length");
            const auto maximum = length.is<std::nullptr_t>() ? 191U : integer(length, 1, 1000000);
            const auto count = std::count_if(source.begin(), source.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xc0U) != 0x80U; });
            if (count > maximum) failed();
        } else if (one_of(type, {"date","time","datetime"})) {
            const auto text = temporal(source, type);
            output = value(object{{"type", value("normalized-value")}, {"version", value(std::int64_t{1})},
                {"kind", value("datetime")}, {"value", value(text)}});
        } else failed();
    }
    // Domain tags are internal provenance. The document output budget measures
    // the emitted scalar; its containing object is charged by document::plan.
    (void)json::encode(output.is<object>() ? output.at("value") : output, output_limit);
    return output;
}
}
