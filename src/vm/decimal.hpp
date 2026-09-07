#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace kumwe::engine::vm {
bool decimal_literal(std::string_view source) noexcept;
class decimal final {
    bool negative = false;
    std::string digits;
    std::size_t scale = 0;
    decimal(bool sign, std::string value, std::size_t places) : negative(sign), digits(std::move(value)), scale(places) {}
    static decimal parts(bool sign, std::string digits, std::size_t scale);
public:
    static decimal parse(std::string_view source);
    std::string literal() const;
    int compare(const decimal& other) const;
    decimal add(const decimal& other) const;
    decimal subtract(const decimal& other) const;
    decimal multiply(const decimal& other, std::uint64_t& budget) const;
    decimal divide(const decimal& other, std::size_t scale, std::uint64_t& budget) const;
};
}
