#pragma once
#include "value/json.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace kumwe::engine::vm {
struct instruction final {
    std::string operation, type, field, lines, aggregate;
    json::value literal;
    std::vector<std::size_t> arguments;
    std::size_t scale = 0;
};
// Post-order bytecode is private and immutable after successful compilation.
class formula final {
    std::vector<instruction> code_;
    json::value document_;
    std::vector<std::string> dependencies_;
    std::map<std::string, std::vector<std::string>> lines_;
public:
    static formula compile(const json::value& expression);
    const json::value& document() const noexcept { return document_; }
    const std::vector<std::string>& dependencies() const noexcept { return dependencies_; }
    const std::map<std::string, std::vector<std::string>>& line_dependencies() const noexcept { return lines_; }
    json::value evaluate(const json::value& fields, const json::value& lines,
                         std::uint64_t& budget, std::size_t output_limit = 16777216) const;
};
}
