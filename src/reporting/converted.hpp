#pragma once
#include <cstdint>
#include <string_view>

namespace kumwe::engine::reporting {
// Reconstruct the portable Conversion value's grammar and arithmetic invariants.
// The accepted scalar remains byte-identical; no host provider is consulted.
bool converted_literal(std::string_view input, bool money, std::uint64_t& budget);
}
