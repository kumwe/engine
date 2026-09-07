#pragma once
#include "value/json.hpp"
#include "vm/formula.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace kumwe::engine::document {
struct field final {
    std::string handle;
    bool required = false;
    bool nullable = true;
    json::value validators;
    std::vector<vm::formula> computation;
};
struct invariant final {
    std::string handle;
    vm::formula condition;
};
// Host codecs supply normalized scalar fields and already resolved owned lines.
// The execution plan contains no storage, identity allocation or authorization.
class plan final {
    std::vector<field> fields_;
    std::vector<invariant> invariants_;
    json::value document_;
    bool require_all_ = true;
public:
    static plan compile(const json::value& program);
    const json::value& document() const noexcept { return document_; }
    json::value execute(const json::value& fields, const json::value& lines,
                        std::uint64_t& budget, std::size_t finding_limit,
                        std::size_t output_limit) const;
};
}
