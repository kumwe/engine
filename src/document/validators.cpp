#include "validators.hpp"
#include "vm/error.hpp"
#include "decimal/decimal.hpp"
#include "value/php_numeric.hpp"
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace kumwe::engine::document {
struct pattern_code final {
    pcre2_code* code = nullptr;
    explicit pattern_code(const std::string& expression, std::uint32_t flags) {
        int error; PCRE2_SIZE offset;
        code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(expression.data()), expression.size(), flags, &error, &offset, nullptr);
        if (code != nullptr) jit = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) == 0;
    }
    bool jit = false;
    ~pattern_code() { pcre2_code_free(code); }
    pattern_code(const pattern_code&) = delete;
    pattern_code& operator=(const pattern_code&) = delete;
};
namespace {
using value = json::value;
using list = value::list;
const value null;
const value& member(const value& v, std::string_view key) { const auto* p = v.find(key); return p ? *p : null; }
bool scalar(const value& v) { return v.is<std::nullptr_t>() || v.is<bool>() || v.is<std::int64_t>() || v.is<std::string>(); }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool alnum(char c) { return alpha(c) || digit(c); }
bool hex(char c) { return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool match(const pattern_code* pattern, const std::string& text, std::uint32_t work, std::uint32_t depth, bool invalid_is_failure) {
    if (pattern == nullptr || pattern->code == nullptr) {
        if (invalid_is_failure) vm::reject("Invalid pattern validator.");
        return false;
    }
    auto* data = pcre2_match_data_create_from_pattern(pattern->code, nullptr);
    auto* context = pcre2_match_context_create(nullptr);
    if (!data || !context) { pcre2_match_data_free(data); pcre2_match_context_free(context); throw std::bad_alloc(); }
    pcre2_set_match_limit(context, work); pcre2_set_depth_limit(context, depth);
    pcre2_set_heap_limit(context, 8192);
    auto* stack = pattern->jit ? pcre2_jit_stack_create(32 * 1024, 192 * 1024, nullptr) : nullptr;
    if (pattern->jit && stack == nullptr) { pcre2_match_data_free(data); pcre2_match_context_free(context); throw std::bad_alloc(); }
    if (stack != nullptr) pcre2_jit_stack_assign(context, nullptr, stack);
    const auto status = pcre2_match(pattern->code, reinterpret_cast<PCRE2_SPTR>(text.data()), text.size(), 0, 0, data, context);
    pcre2_jit_stack_free(stack);
    pcre2_match_data_free(data); pcre2_match_context_free(context);
    if (status >= 0) return true;
    if (status != PCRE2_ERROR_NOMATCH && invalid_is_failure) vm::reject("Pattern validator exhausted or failed.");
    return false;
}
bool uuid(std::string text) {
    // Ramsey GenericValidator removes these exact case-sensitive spellings anywhere.
    for (const auto token : {"urn:", "uuid:", "URN:", "UUID:", "{", "}"}) {
        std::size_t position = 0;
        while ((position = text.find(token, position)) != std::string::npos) text.erase(position, std::string_view(token).size());
    }
    if (text.size() != 36) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (text[i] != '-') return false; }
        else if (!hex(text[i])) return false;
    }
    return true;
}
bool ipv4(std::string_view text) {
    unsigned count = 0;
    while (!text.empty()) {
        const auto end = text.find('.'); const auto part = text.substr(0, end);
        if (part.empty() || part.size() > 3 || (part.size() > 1 && part.front() == '0')) return false;
        unsigned number = 0;
        for (char c : part) { if (!digit(c)) return false; number = number * 10 + static_cast<unsigned>(c - '0'); }
        if (number > 255 || ++count > 4) return false;
        if (end == std::string_view::npos) return count == 4;
        text.remove_prefix(end + 1);
    }
    return false;
}
bool ipv6(std::string_view text) {
    const auto compression = text.find("::");
    if (compression != std::string_view::npos && text.find("::", compression + 2) != std::string_view::npos) return false;
    unsigned groups = 0;
    auto side = [&](std::string_view part, bool final_side) {
        if (part.empty()) return true;
        while (!part.empty()) {
            const auto end = part.find(':'); const auto group = part.substr(0, end);
            if (group.empty()) return false;
            if (group.find('.') != std::string_view::npos) {
                if (!final_side || end != std::string_view::npos || !ipv4(group)) return false;
                groups += 2;
            } else {
                if (group.size() > 4 || !std::all_of(group.begin(), group.end(), hex)) return false;
                ++groups;
            }
            if (end == std::string_view::npos) return true;
            part.remove_prefix(end + 1); if (part.empty()) return false;
        }
        return false;
    };
    if (compression == std::string_view::npos) return side(text, true) && groups == 8;
    return side(text.substr(0, compression), false) && side(text.substr(compression + 2), true) && groups < 8;
}
bool domain(std::string_view host) {
    const bool trailing_dot = !host.empty() && host.back() == '.';
    if (trailing_dot) host.remove_suffix(1);
    if (host.empty() || host.size() > 253 || !alnum(host.front())) return false;
    unsigned width = 0;
    for (std::size_t i = 0; i < host.size(); ++i) {
        const char c = host[i];
        if (c == '.') {
            if (i == 0 || i + 1 == host.size() || !alnum(host[i - 1]) || !alnum(host[i + 1])) return false;
            width = 0;
        } else {
            if (++width > 63 || (!alnum(c) && c != '-')) return false;
            if (c == '-' && i + 1 == host.size() && !trailing_dot) return false;
        }
    }
    return true;
}
bool url(const std::string& text) {
    constexpr std::string_view punctuation = "$-_.+!*'(),{}|\\^~[]`<>#%\";/?:@&=";
    for (char c : text) if (!alnum(c) && punctuation.find(c) == std::string_view::npos) return false;
    const auto colon = text.find(':');
    if (colon == std::string::npos) return false;
    auto scheme = text.substr(0, colon);
    for (char& c : scheme) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if ((scheme != "http" && scheme != "https") || text.substr(colon, 3) != "://") return false;
    const auto start = colon + 3; const auto end = text.find_first_of("/?#", start);
    auto authority = std::string_view(text).substr(start, end == std::string::npos ? end : end - start);
    const auto at = authority.rfind('@');
    if (at != std::string_view::npos) {
        const auto user = authority.substr(0, at); constexpr std::string_view safe = "-._~!$&'()*+,;=:";
        for (std::size_t i = 0; i < user.size(); ++i) {
            const char c = user[i];
            if (alnum(c) || safe.find(c) != std::string_view::npos) continue;
            if (c != '%' || i + 2 >= user.size() || !digit(user[i + 1]) || !hex(user[i + 2])) return false;
            i += 2;
        }
        authority.remove_prefix(at + 1);
    }
    if (authority.empty()) return false;
    if (!(authority.front() == '[' && authority.back() == ']')) {
        const auto port_start = authority.rfind(':');
        if (port_start != std::string_view::npos) {
            auto port = authority.substr(port_start + 1);
            if (port.size() > 5) return false;
            if (!port.empty()) {
                bool negative = port.front() == '-';
                if (negative || port.front() == '+') port.remove_prefix(1);
                if (port.empty() || !digit(port.front())) return false;
                unsigned number = 0;
                for (char c : port) { if (!digit(c)) break; number = number * 10 + static_cast<unsigned>(c - '0'); }
                if (number > 65535 || (negative && number != 0)) return false;
            }
            authority = authority.substr(0, port_start);
        }
    }
    if (authority.size() > 1 && authority.front() == '[' && authority.back() == ']') return ipv6(authority.substr(1, authority.size() - 2));
    return domain(authority);
}
}
#include "email_pattern.hpp"
validator::validator(value source) : source_(std::move(source)) {
    const auto& token = member(source_, "rule");
    if (!token.is<std::string>()) return;
    if (token.as<std::string>() == "email") {
        pattern_ = std::make_shared<pattern_code>(email_pattern, PCRE2_CASELESS | PCRE2_DOLLAR_ENDONLY);
    } else if (token.as<std::string>() == "pattern") {
        const auto& expression = member(source_, "value");
        if (!expression.is<std::string>() || expression.as<std::string>().empty() || expression.as<std::string>().size() > 512) return;
        std::string body = "(*LIMIT_MATCH=100000)(*LIMIT_DEPTH=1000)(?:";
        unsigned backslashes = 0;
        for (char c : expression.as<std::string>()) {
            if (c == '~') {
                // Escaping PHP's delimiter after an odd backslash run creates an
                // unescaped delimiter in the actual wrapper, which PHP refuses.
                if ((backslashes % 2) != 0) return;
                body += "\\~";
            } else body += c;
            backslashes = c == '\\' ? backslashes + 1 : 0;
        }
        body += ')';
        pattern_ = std::make_shared<pattern_code>(body, PCRE2_UTF | PCRE2_UCP | PCRE2_NEVER_BACKSLASH_C | PCRE2_DOLLAR_ENDONLY);
    }
}
std::string validator::name() const {
    const auto& token = member(source_, "rule");
    return token.is<std::string>() ? token.as<std::string>() : "validator_invalid";
}
bool validator::judge(const value& candidate, bool exact_decimal, bool domain_object, const value& definition) const {
    const auto& token = member(source_, "rule");
    if (!token.is<std::string>()) vm::reject("A validator has no rule.");
    const auto& name = token.as<std::string>();
    const auto& argument = member(source_, "value");
    const bool string_value = candidate.is<std::string>() && !domain_object;
    if (name == "decimal") return exact_decimal;
    if (name == "integer") return !domain_object && candidate.is<std::int64_t>();
    if (name == "email") return string_value && candidate.as<std::string>().size() <= 320
        && match(pattern_.get(), candidate.as<std::string>(), 1000000, 100000, false);
    if (name == "url") return string_value && url(candidate.as<std::string>());
    if (name == "uuid") return string_value && uuid(candidate.as<std::string>());
    if (name == "pattern") return string_value && match(pattern_.get(), candidate.as<std::string>(), 100000, 1000, true);
    if (name == "min_length" || name == "max_length") {
        if (!string_value) return false;
        if (!argument.is<std::int64_t>() || argument.as<std::int64_t>() < 0 || argument.as<std::int64_t>() > 1000000)
            vm::reject("Invalid length bound.");
        const auto& text = candidate.as<std::string>();
        const auto count = std::count_if(text.begin(), text.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xc0U) != 0x80U; });
        return name == "min_length" ? count >= argument.as<std::int64_t>() : count <= argument.as<std::int64_t>();
    }
    if (name == "one_of") {
        if (!argument.is<list>() || argument.as<list>().empty() || argument.as<list>().size() > 256) vm::reject("Invalid option list.");
        for (const auto& option : argument.as<list>()) if (!scalar(option)) vm::reject("Invalid option value.");
        return std::find(argument.as<list>().begin(), argument.as<list>().end(), candidate) != argument.as<list>().end();
    }
    if (name == "min" || name == "max") {
        if (!scalar(argument)) vm::reject("Invalid range bound.");
        int order = 0;
        if (exact_decimal) {
            const auto& kind = member(definition, "type");
            const auto& precision = member(definition, "precision"); const auto& scale = member(definition, "scale");
            const auto& formula = member(definition, "formula");
            const bool decimal_field = kind == value("core.decimal") || (kind == value("core.computed") && member(formula, "type") == value("decimal"));
            if (!decimal_field || !precision.is<std::int64_t>() || !scale.is<std::int64_t>() || precision.as<std::int64_t>() < 1
                || precision.as<std::int64_t>() > 65 || scale.as<std::int64_t>() < 0 || scale.as<std::int64_t>() > precision.as<std::int64_t>()
                || (!argument.is<std::string>() && !argument.is<std::int64_t>())) vm::reject("Incompatible exact range bound.");
            try {
                const auto p = static_cast<unsigned>(precision.as<std::int64_t>()), s = static_cast<unsigned>(scale.as<std::int64_t>());
                const auto& literal = candidate.as<std::string>();
                const auto point = literal.find('.');
                const auto actual_scale = point == std::string::npos ? 0 : literal.size() - point - 1;
                const auto left = decimal::value::parse(literal, 65, static_cast<unsigned>(actual_scale));
                const auto right = decimal::value::parse(argument.is<std::string>() ? argument.as<std::string>() : std::to_string(argument.as<std::int64_t>()), p, s);
                order = left.compare(right);
            } catch (const decimal::invalid_decimal&) { vm::reject("Invalid exact range bound."); }
        } else if (candidate.is<std::int64_t>() && argument.is<std::string>()) {
            const auto& digits = argument.as<std::string>(); std::int64_t limit = 0;
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), limit);
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || digits.empty()
                || (digits.size() > 1 && digits[0] == '0') || (digits.size() > 2 && digits[0] == '-' && digits[1] == '0')) vm::reject("Invalid integer bound.");
            order = candidate.as<std::int64_t>() == limit ? 0 : candidate.as<std::int64_t>() < limit ? -1 : 1;
        } else if (string_value && argument.is<std::string>()) {
            order = value_compat::php_string_compare(candidate.as<std::string>(), argument.as<std::string>());
        } else vm::reject("Invalid range bound.");
        return name == "min" ? order >= 0 : order <= 0;
    }
    vm::reject("Unregistered validator.");
}
}
