#include "converted.hpp"
#include "decimal/decimal.hpp"
#include "batch.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <array>

namespace kumwe::engine::reporting {
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool lower(char c) { return c >= 'a' && c <= 'z'; }
bool upper(char c) { return c >= 'A' && c <= 'Z'; }
bool denomination(std::string_view text, bool money) {
    if (money) return text.size() == 3 && std::all_of(text.begin(), text.end(), upper);
    if (text.empty() || text.size() > 63 || !(digit(text.front()) || lower(text.front()) || upper(text.front()))) return false;
    for (const char c : text) if (!(digit(c) || lower(c) || upper(c) || c == '.' || c == '_' || c == '/' || c == '-')) return false;
    return true;
}
bool provider(std::string_view text) {
    if (text.empty() || !lower(text.front())) return false;
    unsigned separators = 0;
    bool previous_separator = false;
    for (const char c : text) {
        if (lower(c) || digit(c)) previous_separator = false;
        else if ((c == '.' || c == '_' || c == '-') && !previous_separator) {
            ++separators; previous_separator = true;
        } else return false;
    }
    return !previous_separator && separators >= 1 && separators <= 15;
}
bool instant(std::string_view text) {
    if (text.size() != 32 || text[4] != '-' || text[7] != '-' || text[10] != 'T'
        || text[13] != ':' || text[16] != ':' || text[19] != '.' || text.substr(26) != "+00:00") return false;
    for (std::size_t i = 0; i < 26; ++i) {
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && i != 19 && !digit(text[i])) return false;
    }
    const auto part = [&](std::size_t start, std::size_t width) {
        unsigned result = 0;
        for (std::size_t i = start; i < start + width; ++i) result = result * 10 + static_cast<unsigned>(text[i] - '0');
        return result;
    };
    const auto year = part(0, 4), month = part(5, 2), day = part(8, 2);
    if (month < 1 || month > 12 || day < 1 || part(11, 2) > 23 || part(14, 2) > 59 || part(17, 2) > 59) return false;
    constexpr std::array<unsigned, 12> days{31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day <= days[month - 1] + (month == 2 && leap ? 1U : 0U);
}
struct exact final { decimal::value number; unsigned precision; };
exact literal(std::string_view text) {
    const auto unsigned_text = text.starts_with('-') ? text.substr(1) : text;
    const auto point = unsigned_text.find('.');
    const auto integer = unsigned_text.substr(0, point);
    const auto scale = point == std::string_view::npos ? 0U : static_cast<unsigned>(unsigned_text.size() - point - 1);
    const auto precision = std::max(1U, static_cast<unsigned>(integer == "0" ? 0 : integer.size()) + scale);
    return {decimal::value::parse(text, precision, scale), precision};
}
}
bool converted_literal(std::string_view input, bool money, std::uint64_t& budget) {
    // Every valid portable token is ASCII, so this also enforces the owner's
    // 512-character bound without widening the accepted grammar.
    if (input.size() > 512) return false;
    constexpr std::uint64_t work = 512 + 4356 + 512;
    if (work > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= work;
    std::array<std::string_view, 17> tokens;
    auto rest = input;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto space = rest.find(' ');
        if ((i + 1 == tokens.size()) != (space == std::string_view::npos)) return false;
        tokens[i] = rest.substr(0, space);
        if (tokens[i].empty()) return false;
        if (space != std::string_view::npos) rest.remove_prefix(space + 1);
    }
    if (tokens[2] != "converted" || tokens[3] != "from" || tokens[6] != "at" || tokens[8] != "as"
        || tokens[9] != "at" || tokens[11] != "by" || tokens[13] != "rounded" || tokens[15] != "from") return false;
    const auto target_unit = tokens[money ? 0U : 1U], source_unit = tokens[money ? 4U : 5U];
    if (!denomination(target_unit, money) || !denomination(source_unit, money) || target_unit == source_unit
        || !instant(tokens[10]) || !provider(tokens[12])) return false;
    constexpr std::array<std::string_view, 6> modes{"half_up","half_down","half_even","ceiling","floor","truncate"};
    const auto mode = std::find(modes.begin(), modes.end(), tokens[14]);
    if (mode == modes.end() || tokens[7].starts_with('-') || tokens[7].find('.') == std::string_view::npos
        || tokens[7].find_first_not_of("0.") == std::string_view::npos) return false;
    try {
        const auto target = literal(tokens[money ? 1U : 0U]);
        const auto source = literal(tokens[money ? 5U : 4U]);
        const auto factor = literal(tokens[7]);
        const auto unrounded = literal(tokens[16]);
        const auto product = source.number.multiply(factor.number);
        if (product.literal() != unrounded.number.literal()) return false;
        const auto rounded = product.round(target.precision, target.number.scale(),
            static_cast<decimal::rounding>(mode - modes.begin()));
        return rounded.literal() == target.number.literal();
    } catch (const decimal::invalid_decimal&) { return false; }
}
}
