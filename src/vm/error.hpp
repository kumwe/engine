#pragma once
#include <stdexcept>
#include <string>
namespace kumwe::engine::vm {
// Semantic refusal text is retained for corpus diagnostics only. The ABI emits status codes.
class error final : public std::runtime_error {
public:
    error(bool parsing, const std::string& message) : std::runtime_error(message), parse_phase(parsing) {}
    const bool parse_phase;
};
[[noreturn]] inline void reject(const std::string& message) { throw error(false, message); }
}
