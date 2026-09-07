#include "decimal.hpp"
#include <algorithm>
#include <array>
#include <utility>
namespace kumwe::engine::decimal {
namespace {
bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
std::string digits(std::string_view literal) {
    std::string result;
    result.reserve(literal.size());
    for (char c : literal) if (digit(c)) result.push_back(c);
    return result;
}
std::string_view significant(std::string_view input) noexcept {
    const auto first = input.find_first_not_of('0');
    return first == std::string_view::npos ? input.substr(input.size() - 1) : input.substr(first);
}
std::string spell(std::string number, unsigned scale, bool negative) {
    if (number.empty()) number = "0";
    if (number.size() <= scale) number.insert(0, scale + 1 - number.size(), '0');
    const auto split = number.size() - scale;
    std::string result(significant(std::string_view(number).substr(0, split)));
    if (scale != 0) result += "." + number.substr(split);
    if (negative) result.insert(0, 1, '-');
    return result;
}
void increment(std::string& number) {
    for (auto i = number.size(); i != 0; --i) {
        if (number[i - 1] != '9') { ++number[i - 1]; return; }
        number[i - 1] = '0';
    }
    number.insert(0, 1, '1');
}
}
value value::parse(std::string_view input, unsigned precision, unsigned scale) {
    if (precision < 1 || precision > 65 || scale > precision || input.empty() || input.size() > 68)
        throw invalid_decimal{};
    const bool negative = input.front() == '-';
    auto unsigned_input = negative ? input.substr(1) : input;
    if (unsigned_input.empty()) throw invalid_decimal{};
    const auto point = unsigned_input.find('.');
    const auto integer = unsigned_input.substr(0, point);
    const auto fraction = point == std::string_view::npos ? std::string_view{} : unsigned_input.substr(point + 1);
    if (integer.empty() || (integer.size() > 1 && integer.front() == '0') ||
        (point != std::string_view::npos && fraction.empty()) || fraction.size() > scale)
        throw invalid_decimal{};
    for (char c : integer) if (!digit(c)) throw invalid_decimal{};
    for (char c : fraction) if (!digit(c)) throw invalid_decimal{};
    const auto integer_digits = integer == "0" ? 0 : integer.size();
    if (integer_digits > precision - scale) throw invalid_decimal{};
    const bool zero = integer == "0" && fraction.find_first_not_of('0') == std::string_view::npos;
    std::string result;
    result.reserve(68);
    if (negative && !zero) result.push_back('-');
    result += integer;
    if (scale != 0) { result.push_back('.'); result += fraction; result.append(scale - fraction.size(), '0'); }
    return value(std::move(result), scale);
}
int value::compare(const value& right) const {
    if (scale_ != right.scale_) throw invalid_decimal{};
    const bool left_negative = literal_.front() == '-';
    const bool right_negative = right.literal_.front() == '-';
    if (left_negative != right_negative) return left_negative ? -1 : 1;
    const auto left_digits = digits(literal_);
    const auto right_digits = digits(right.literal_);
    const auto left = significant(left_digits);
    const auto other = significant(right_digits);
    int result = left.size() == other.size() ? (left == other ? 0 : (left < other ? -1 : 1)) :
        (left.size() < other.size() ? -1 : 1);
    return left_negative ? -result : result;
}
value value::multiply(const value& right) const {
    const unsigned scale = scale_ + right.scale_;
    if (scale > 65) throw invalid_decimal{};
    const auto left = digits(literal_);
    const auto other = digits(right.literal_);
    // Maximum 66 digits including the required zero before a fractional decimal.
    std::array<unsigned, 132> product{};
    const auto count = left.size() + other.size();
    for (auto i = left.size(); i != 0; --i) {
        unsigned carry = 0;
        for (auto j = other.size(); j != 0; --j) {
            const auto position = i + j - 1;
            const unsigned total = product[position] + static_cast<unsigned>(left[i - 1] - '0') *
                static_cast<unsigned>(other[j - 1] - '0') + carry;
            product[position] = total % 10;
            carry = total / 10;
        }
        product[i - 1] += carry;
    }
    std::string result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) result.push_back(static_cast<char>('0' + product[i]));
    result = std::string(significant(result));
    return parse(spell(std::move(result), scale, (literal_.front() == '-') != (right.literal_.front() == '-')), 65, scale);
}
value value::round(unsigned precision, unsigned scale, rounding mode) const {
    if (precision < 1 || precision > 65 || scale > precision || static_cast<unsigned>(mode) > 5)
        throw invalid_decimal{};
    if (scale >= scale_) return parse(literal_, precision, scale);
    auto number = digits(literal_);
    const auto keep = number.size() - (scale_ - scale);
    const unsigned first = static_cast<unsigned>(number[keep] - '0');
    const bool rest = number.find_first_not_of('0', keep + 1) != std::string::npos;
    const bool odd = keep != 0 && ((number[keep - 1] - '0') % 2 != 0);
    const bool negative = literal_.front() == '-';
    bool grow = false;
    switch (mode) {
    case rounding::half_up: grow = first >= 5; break;
    case rounding::half_down: grow = first > 5 || (first == 5 && rest); break;
    case rounding::half_even: grow = first > 5 || (first == 5 && (rest || odd)); break;
    case rounding::ceiling: grow = (first != 0 || rest) && !negative; break;
    case rounding::floor: grow = (first != 0 || rest) && negative; break;
    case rounding::truncate: break;
    }
    number.resize(keep);
    if (grow) increment(number);
    return parse(spell(std::move(number), scale, negative), precision, scale);
}
}
