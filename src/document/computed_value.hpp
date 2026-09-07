#pragma once
#include "value/json.hpp"
#include <cstddef>
#include <cstdint>

namespace kumwe::engine::document {
void validate_computed_definition(const json::value& definition);
// Preserve object provenance until the document layer produces expression values.
// Normalization errors are vm::error; resource refusal remains a whole-call refusal.
json::value normalize_computed_value(json::value output, const json::value& definition,
                                    std::uint64_t& budget, std::size_t output_limit);
}
