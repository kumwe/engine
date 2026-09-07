#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
namespace kumwe::engine::decimal {
class invalid_decimal final : public std::exception {};
enum class rounding : std::uint8_t { half_up, half_down, half_even, ceiling, floor, truncate };
class value final {
public:
    static value parse(std::string_view input, unsigned precision, unsigned scale);
    [[nodiscard]] const std::string& literal() const noexcept { return literal_; }
    [[nodiscard]] unsigned scale() const noexcept { return scale_; }
    [[nodiscard]] int compare(const value& right) const;
    [[nodiscard]] value multiply(const value& right) const;
    [[nodiscard]] value round(unsigned precision, unsigned scale, rounding mode) const;
private:
    value(std::string literal, unsigned scale) : literal_(std::move(literal)), scale_(scale) {}
    std::string literal_;
    unsigned scale_;
};
}
