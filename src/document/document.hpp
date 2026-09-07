#pragma once
#include "value/json.hpp"
#include "vm/formula.hpp"
#include "validators.hpp"
#include <functional>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace kumwe::engine::document {
// Bounded, host-normalized transport; these helpers never invoke a host codec.
bool normalized_value_valid(const json::value& value);
void validate_normalized_instances(const std::vector<const json::value*>& inputs);
bool normalized_domain_object(const json::value& value);
bool normalized_strict_equal(const json::value& current, const json::value& submitted);
bool normalized_exact_decimal(const json::value& value);
json::value expression_value(const json::value& value);
json::value output_value(const json::value& value);
using finding_sink = std::function<void(const std::string&, const std::string&)>;
struct execution_context final {
    json::value::list initial_findings;
    std::function<void(const json::value&, std::uint64_t&, const finding_sink&)> after_computation;
};
struct field final {
    std::string handle;
    bool required = false;
    bool nullable = true;
    json::value definition;
    std::vector<validator> validators;
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
    std::set<std::string, std::less<>> projected_fields_;
    json::value document_;
    bool require_all_ = true;
    json::value execute_impl(const json::value& fields, const json::value& lines,
        std::uint64_t& budget, std::size_t finding_limit, std::size_t output_limit,
        const execution_context* context, json::value* owned_fields) const;
public:
    static plan compile(const json::value& program);
    const json::value& document() const noexcept { return document_; }
    json::value execute(const json::value& fields, const json::value& lines,
                        std::uint64_t& budget, std::size_t finding_limit,
                        std::size_t output_limit, const execution_context* context = nullptr) const;
    json::value execute_owned(json::value&& fields, const json::value& lines,
                        std::uint64_t& budget, std::size_t finding_limit,
                        std::size_t output_limit, const execution_context* context = nullptr) const;
};
}
