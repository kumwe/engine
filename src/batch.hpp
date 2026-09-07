#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace kumwe::engine {
class refusal final {
public:
    explicit refusal(std::uint32_t code) : code(code) {}
    std::uint32_t code;
};
std::string decimal_batch(std::string_view request);
std::string capabilities(std::string_view request);
}
