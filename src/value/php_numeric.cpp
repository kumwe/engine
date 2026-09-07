#include "php_numeric.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>

namespace kumwe::engine::value_compat {
namespace {
// Binary64 is confined to compatibility with PHP's numeric STRING comparator.
// Financial values and canonical-decimal report operands never enter this branch.
struct numeric final {
    bool integer = false, integer_overflow = false;
    std::int64_t whole = 0;
    double approximate = 0;
};
bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool binary64(std::string_view source, double& output) {
    // Floating from_chars is absent from the supported AppleClang 15 libc++.
    // The grammar is checked before this conversion; an explicit classic
    // locale keeps the portable conversion independent of the caller's locale.
    std::istringstream stream{std::string(source)};
    stream.imbue(std::locale::classic());
    stream >> output;
    // libc++ marks ERANGE underflow as failbit while preserving strtod's
    // correctly rounded subnormal. Keep that value, including signed zero.
    return !stream.fail() || std::abs(output) <= std::numeric_limits<double>::min();
}
std::optional<numeric> parse(std::string_view source) {
    while (!source.empty() && space(source.front())) source.remove_prefix(1);
    while (!source.empty() && space(source.back())) source.remove_suffix(1);
    if (source.empty()) return std::nullopt;
    const bool negative = source.front() == '-';
    if (source.front() == '+') source.remove_prefix(1);
    if (source.empty()) return std::nullopt;
    std::size_t cursor = negative ? 1 : 0;
    const auto start = cursor;
    while (cursor < source.size() && digit(source[cursor])) ++cursor;
    const auto integral_digits = cursor - start;
    bool fraction = false, exponent = false;
    std::size_t fractional_digits = 0;
    if (cursor < source.size() && source[cursor] == '.') {
        fraction = true; ++cursor;
        const auto first = cursor;
        while (cursor < source.size() && digit(source[cursor])) ++cursor;
        fractional_digits = cursor - first;
    }
    if (integral_digits + fractional_digits == 0) return std::nullopt;
    std::int64_t power = 0;
    if (cursor < source.size() && (source[cursor] == 'e' || source[cursor] == 'E')) {
        exponent = true; ++cursor;
        bool minus = false;
        if (cursor < source.size() && (source[cursor] == '+' || source[cursor] == '-')) { minus = source[cursor] == '-'; ++cursor; }
        const auto first = cursor;
        while (cursor < source.size() && digit(source[cursor])) {
            if (power < 1000000000) power = power * 10 + source[cursor] - '0';
            ++cursor;
        }
        if (cursor == first) return std::nullopt;
        if (minus) power = -power;
    }
    if (cursor != source.size()) return std::nullopt;
    numeric result;
    if (!fraction && !exponent) {
        // from_chars accepts no leading plus; it was removed above.
        const auto parsed = std::from_chars(source.data(),source.data()+source.size(),result.whole);
        if (parsed.ec == std::errc{} && parsed.ptr == source.data()+source.size()) {
            result.integer = true;
            (void)binary64(source, result.approximate);
            return result;
        }
        result.integer_overflow = true;
    }
    if (binary64(source, result.approximate)) return result;
    // A valid numeric spelling can fail conversion only outside the binary64
    // range. Distinguish underflow from overflow by the first nonzero digit's order.
    std::size_t index = 0;
    bool nonzero = false;
    for (std::size_t i = start; i < source.size() && source[i] != 'e' && source[i] != 'E'; ++i) {
        if (source[i] == '.') continue;
        if (source[i] != '0') { nonzero = true; break; }
        ++index;
    }
    const auto order = power + static_cast<std::int64_t>(integral_digits) - static_cast<std::int64_t>(index) - 1;
    result.approximate = nonzero && order >= 0 ? std::numeric_limits<double>::infinity() : 0.0;
    if (negative) result.approximate = -result.approximate;
    return result;
}
int lexical(std::string_view left, std::string_view right) { return left == right ? 0 : left < right ? -1 : 1; }
}
int php_string_compare(std::string_view left, std::string_view right) {
    const auto l = parse(left), r = parse(right);
    if (!l || !r) return lexical(left,right);
    if (l->integer && r->integer) return l->whole == r->whole ? 0 : l->whole < r->whole ? -1 : 1;
    if (l->integer_overflow && r->integer) return l->approximate < 0 ? -1 : 1;
    if (r->integer_overflow && l->integer) return r->approximate < 0 ? 1 : -1;
    const double a = l->approximate;
    const double b = r->approximate;
    if (a == b) {
        // PHP retains byte ordering when distinct overflowing integer strings
        // lose precision, and when two exponential strings become one infinity.
        if ((l->integer_overflow && r->integer_overflow) || !std::isfinite(a)) return lexical(left,right);
        return 0;
    }
    return a < b ? -1 : 1;
}
}
