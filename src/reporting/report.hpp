#pragma once
#include "value/json.hpp"
#include "vm/formula.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace kumwe::engine::reporting {
// Computation consumes already authorized, projected, normalized scalar rows.
// No source resolver, query compiler, capability, storage or delivery hook exists.
class report_plan final {
    struct aggregate final { std::string alias, function, column; };
    struct formula final { std::string alias, type; vm::formula expression; };
    struct sort final { std::string output; bool descending = false, nulls_last = true; };
    json::value document_;
    std::vector<std::string> groups_;
    std::vector<aggregate> aggregates_;
    std::vector<formula> formulas_;
    std::vector<sort> sorts_;
public:
    static report_plan compile(const json::value& computation);
    const json::value& document() const noexcept { return document_; }
    json::value materialize(const json::value& authorized_rows, std::uint64_t& budget,
                            std::size_t max_rows = 100000,
                            std::size_t max_output_bytes = 16777216) const;
};
}
