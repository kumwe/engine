#include "decimal.hpp"
#include "error.hpp"
#include "batch.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <vector>
namespace kumwe::engine::vm {
namespace {
std::string trim_zero(std::string digits) {
    const auto first = digits.find_first_not_of('0');
    return first == std::string::npos ? "0" : digits.substr(first);
}
int compare_abs(std::string_view left, std::string_view right) {
    while (left.size() > 1 && left.front() == '0') left.remove_prefix(1);
    while (right.size() > 1 && right.front() == '0') right.remove_prefix(1);
    if (left.empty()) left = "0";
    if (right.empty()) right = "0";
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    return left == right ? 0 : left < right ? -1 : 1;
}
std::string add_abs(std::string_view left, std::string_view right) {
    const auto width = std::max(left.size(), right.size());
    std::string output(width + 1, '0'); unsigned carry = 0;
    for (std::size_t offset = 0; offset < width; ++offset) {
        unsigned total = carry;
        if (offset < left.size()) total += static_cast<unsigned>(left[left.size() - offset - 1] - '0');
        if (offset < right.size()) total += static_cast<unsigned>(right[right.size() - offset - 1] - '0');
        output[width - offset] = static_cast<char>('0' + total % 10); carry = total / 10;
    }
    output[0] = static_cast<char>('0' + carry); return trim_zero(std::move(output));
}
std::string subtract_abs(std::string_view left, std::string_view right) {
    std::string output(left); int borrow = 0;
    for (std::size_t offset = 0; offset < left.size(); ++offset) {
        int next = left[left.size() - offset - 1] - '0' - borrow;
        if (offset < right.size()) next -= right[right.size() - offset - 1] - '0';
        borrow = next < 0 ? 1 : 0; if (next < 0) next += 10;
        output[left.size() - offset - 1] = static_cast<char>('0' + next);
    }
    return trim_zero(std::move(output));
}
void charge(std::uint64_t& budget, std::uint64_t cost) {
    if (cost > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= cost;
}
}
bool decimal_literal(std::string_view source) noexcept {
    if (source.starts_with('-')) source.remove_prefix(1);
    if (source.empty()) return false;
    if (source.front() == '0') source.remove_prefix(1);
    else {
        if (source.front() < '1' || source.front() > '9') return false;
        do { source.remove_prefix(1); } while (!source.empty() && source.front() >= '0' && source.front() <= '9');
    }
    if (source.empty()) return true;
    if (source.front() != '.') return false;
    source.remove_prefix(1); if (source.empty()) return false;
    for (const char c : source) if (c < '0' || c > '9') return false;
    return true;
}
decimal decimal::parse(std::string_view source) {
    constexpr std::string_view whitespace(" \t\n\r\0\x0b", 6);
    while (!source.empty() && whitespace.find(source.front()) != std::string_view::npos) source.remove_prefix(1);
    while (!source.empty() && whitespace.find(source.back()) != std::string_view::npos) source.remove_suffix(1);
    if (!decimal_literal(source)) reject("A decimal value must be a canonical base-10 string.");
    const bool sign = source.starts_with('-'); if (sign) source.remove_prefix(1);
    const auto dot = source.find('.');
    const auto places = dot == std::string_view::npos ? 0 : source.size() - dot - 1;
    if (source.size() - (dot == std::string_view::npos ? 0U : 1U) > 4096) reject("A decimal value exceeds 4096 digits.");
    std::string value(source); if (dot != std::string_view::npos) value.erase(dot, 1);
    value = trim_zero(std::move(value));
    const bool result_sign = sign && value != "0";
    return decimal(result_sign, std::move(value), places);
}
decimal decimal::parts(bool sign, std::string value, std::size_t places) {
    value = trim_zero(std::move(value));
    if (value.size() > 4096) reject("A decimal result exceeds 4096 digits.");
    // Deliberately preserve the owner corpus's historical zero/scale behavior.
    while (places > 0 && !value.empty() && value.back() == '0') { value.pop_back(); --places; }
    const bool result_sign = sign && value != "0";
    return decimal(result_sign, std::move(value), places);
}
std::string decimal::literal() const {
    auto value = digits;
    if (scale > 0) {
        if (value.size() <= scale) value.insert(0, scale + 1 - value.size(), '0');
        value.insert(value.size() - scale, 1, '.');
    }
    return (negative ? "-" : "") + value;
}
int decimal::compare(const decimal& other) const {
    if (negative != other.negative) return negative ? -1 : 1;
    const auto places = std::max(scale, other.scale);
    const auto left = digits + std::string(places - scale, '0');
    const auto right = other.digits + std::string(places - other.scale, '0');
    const int result = compare_abs(left, right); return negative ? -result : result;
}
decimal decimal::add(const decimal& other) const {
    const auto places = std::max(scale, other.scale);
    const auto left = digits + std::string(places - scale, '0');
    const auto right = other.digits + std::string(places - other.scale, '0');
    if (negative == other.negative) return parts(negative, add_abs(left, right), places);
    const auto compare = compare_abs(left, right);
    if (compare == 0) return parts(false, "0", places);
    return compare > 0 ? parts(negative, subtract_abs(left, right), places) : parts(other.negative, subtract_abs(right, left), places);
}
decimal decimal::subtract(const decimal& other) const { return add(decimal(!other.negative && other.digits != "0", other.digits, other.scale)); }
decimal decimal::multiply(const decimal& other, std::uint64_t& budget) const {
    charge(budget, digits.size() * other.digits.size() + digits.size() + other.digits.size());
    std::vector<unsigned> product(digits.size() + other.digits.size(), 0);
    for (std::size_t i = 0; i < digits.size(); ++i)
        for (std::size_t j = 0; j < other.digits.size(); ++j)
            product[i + j] += static_cast<unsigned>(digits[digits.size() - i - 1] - '0') * static_cast<unsigned>(other.digits[other.digits.size() - j - 1] - '0');
    for (std::size_t i = 0; i + 1 < product.size(); ++i) { product[i + 1] += product[i] / 10; product[i] %= 10; }
    std::string value; value.reserve(product.size());
    for (auto i = product.rbegin(); i != product.rend(); ++i) value.push_back(static_cast<char>('0' + *i));
    return parts(negative != other.negative, std::move(value), scale + other.scale);
}
decimal decimal::divide(const decimal& other, std::size_t places, std::uint64_t& budget) const {
    if (places > 30) reject("Decimal division scale must be between 0 and 30.");
    if (other.digits == "0") reject("A definition formula attempted division by zero.");
    const auto left_places = places + other.scale;
    const auto numerator = digits + std::string(left_places > scale ? left_places - scale : 0, '0');
    const auto denominator = other.digits + std::string(scale > left_places ? scale - left_places : 0, '0');
    charge(budget, numerator.size() * (denominator.size() + 1) * 10);
    std::string quotient, remainder = "0"; quotient.reserve(numerator.size());
    for (const char digit : numerator) {
        remainder = trim_zero(remainder + digit); unsigned next = 0;
        while (compare_abs(remainder, denominator) >= 0) { remainder = subtract_abs(remainder, denominator); ++next; }
        quotient.push_back(static_cast<char>('0' + next));
    }
    quotient = trim_zero(std::move(quotient));
    if (compare_abs(add_abs(remainder, remainder), denominator) >= 0) quotient = add_abs(quotient, "1");
    return parts(negative != other.negative, std::move(quotient), places);
}
}
