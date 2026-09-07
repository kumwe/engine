#include "unicode_normalization.hpp"
#include "unicode_data.hpp"
#include "batch.hpp"
#include "vm/error.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <vector>

namespace kumwe::engine::document {
namespace {
using point = std::uint32_t;
void charge(std::uint64_t& budget, std::uint64_t amount = 1) {
    if (amount > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= amount;
}
[[noreturn]] void invalid() { vm::reject("Unicode field normalization requires valid UTF-8 and a known operation."); }
point decode(std::string_view text, std::size_t& offset, std::uint64_t& budget) {
    charge(budget);
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 0x80) return first;
    unsigned remaining;
    point result, minimum;
    if (first >= 0xc2 && first <= 0xdf) { remaining = 1; result = first & 0x1fU; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { remaining = 2; result = first & 0x0fU; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; result = first & 0x07U; minimum = 0x10000; }
    else invalid();
    if (text.size() - offset < remaining) invalid();
    while (remaining-- != 0) {
        charge(budget);
        const auto next = static_cast<unsigned char>(text[offset++]);
        if ((next & 0xc0U) != 0x80U) invalid();
        result = (result << 6) | (next & 0x3fU);
    }
    if (result < minimum || result > 0x10ffff || (result >= 0xd800 && result <= 0xdfff)) invalid();
    return result;
}
void append(std::string& output, point cp, std::uint64_t& budget, std::size_t limit) {
    const std::size_t width = cp < 0x80 ? 1U : (cp < 0x800 ? 2U : (cp < 0x10000 ? 3U : 4U));
    if (width > limit - output.size()) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    charge(budget, width);
    if (width == 1) output.push_back(static_cast<char>(cp));
    else {
        if (width == 2) output.push_back(static_cast<char>(0xc0U | (cp >> 6)));
        else if (width == 3) {
            output.push_back(static_cast<char>(0xe0U | (cp >> 12)));
            output.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xf0U | (cp >> 18)));
            output.push_back(static_cast<char>(0x80U | ((cp >> 12) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3fU)));
        }
        output.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    }
}
template <typename Entry, std::size_t Size>
const Entry* lookup(const Entry (&entries)[Size], point cp) {
    const auto* end = entries + Size;
    const auto* found = std::lower_bound(entries, end, cp, [](const Entry& item, point key) { return item.point < key; });
    return found != end && found->point == cp ? found : nullptr;
}
template <std::size_t Size>
bool contains(const unicode_data::range (&entries)[Size], point cp) {
    const auto* end = entries + Size;
    const auto* found = std::lower_bound(entries, end, cp, [](const auto& item, point key) { return item.last < key; });
    return found != end && found->first <= cp;
}
unsigned combining(point cp) {
    const auto* item = lookup(unicode_data::classes, cp);
    return item == nullptr ? 0U : item->value;
}
bool followed_by_cased(std::string_view text, std::size_t offset, std::uint64_t& budget) {
    while (offset < text.size()) {
        const auto cp = decode(text, offset, budget);
        if (!contains(unicode_data::case_ignorable, cp)) return contains(unicode_data::cased, cp);
    }
    return false;
}
std::string casing(std::string_view text, bool lower, std::uint64_t& budget, std::size_t limit) {
    std::string output;
    bool preceding_cased = false;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto cp = decode(text, offset, budget);
        const auto* mapping = lower ? lookup(unicode_data::lowercase, cp) : lookup(unicode_data::uppercase, cp);
        // Final_Sigma uses the original string and possessive Case_Ignorable runs.
        if (lower && cp == 0x03a3 && preceding_cased && !followed_by_cased(text, offset, budget))
            append(output, 0x03c2, budget, limit);
        else if (mapping != nullptr) {
            for (unsigned i = 0; i < mapping->size; ++i) append(output, mapping->values[i], budget, limit);
        } else append(output, cp, budget, limit);
        if (!contains(unicode_data::case_ignorable, cp)) preceding_cased = contains(unicode_data::cased, cp);
    }
    return output;
}
point composition(point first, point second) {
    // UAX #15's algorithmic Hangul composition; no table for its 11,172 syllables.
    if (first >= 0x1100 && first < 0x1100 + 19 && second >= 0x1161 && second < 0x1161 + 21)
        return 0xac00 + ((first - 0x1100) * 21 + (second - 0x1161)) * 28;
    if (first >= 0xac00 && first < 0xac00 + 11172 && (first - 0xac00) % 28 == 0
        && second > 0x11a7 && second < 0x11a7 + 28) return first + second - 0x11a7;
    const std::uint64_t pair = (static_cast<std::uint64_t>(first) << 21) | second;
    const auto* begin = unicode_data::compositions;
    const auto* end = begin + sizeof(unicode_data::compositions) / sizeof(*begin);
    const auto* found = std::lower_bound(begin, end, pair, [](const auto& item, std::uint64_t key) { return item.pair < key; });
    return found != end && found->pair == pair ? found->point : 0;
}
std::string nfc(std::string_view text, std::uint64_t& budget, std::size_t limit) {
    std::vector<point> points;
    const auto emit = [&](point cp) {
        // Charge retained intermediate storage before allocation. Canonical decomposition is at most four points.
        // Any NFC result therefore needs at least one output byte per four decomposed points.
        if (points.size() >= limit * 4) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        charge(budget, sizeof(point));
        points.push_back(cp);
        const auto order = combining(cp);
        if (order == 0) return;
        auto position = points.size() - 1;
        while (position > 0) {
            charge(budget);
            const auto previous = combining(points[position - 1]);
            if (previous == 0 || previous <= order) break;
            std::swap(points[position], points[position - 1]);
            --position;
        }
    };
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto cp = decode(text, offset, budget);
        if (cp >= 0xac00 && cp < 0xac00 + 11172) {
            const auto index = cp - 0xac00;
            emit(0x1100 + index / (21 * 28));
            emit(0x1161 + (index % (21 * 28)) / 28);
            if (index % 28 != 0) emit(0x11a7 + index % 28);
        } else if (const auto* mapping = lookup(unicode_data::decomposition, cp)) {
            for (unsigned i = 0; i < mapping->size; ++i) emit(mapping->values[i]);
        } else emit(cp);
    }
    std::size_t retained = 0, starter = 0;
    bool has_starter = false;
    unsigned previous_class = 0;
    for (const auto cp : points) {
        charge(budget);
        const auto current_class = combining(cp);
        const auto composite = has_starter && (previous_class == 0 || previous_class < current_class)
            ? composition(points[starter], cp) : 0;
        if (composite != 0) points[starter] = composite;
        else {
            if (current_class == 0) { starter = retained; has_starter = true; }
            points[retained++] = cp;
            previous_class = current_class;
        }
    }
    std::string output;
    for (std::size_t i = 0; i < retained; ++i) append(output, points[i], budget, limit);
    return output;
}
}
std::string normalize_unicode(std::string_view text, std::string_view operation,
                              std::uint64_t& budget, std::size_t output_limit) {
    if (text.size() > 67108864 || output_limit > 67108864) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    if (operation == "lowercase") return casing(text, true, budget, output_limit);
    if (operation == "uppercase") return casing(text, false, budget, output_limit);
    if (operation == "unicode_nfc") return nfc(text, budget, output_limit);
    invalid();
}
}
