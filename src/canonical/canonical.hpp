#pragma once
#include "value/json.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace kumwe::engine::canonical {
struct binary64 final { std::uint64_t bits; };
struct unsupported final {};
struct value final {
    using key = std::variant<std::int64_t, std::string>;
    using array = std::vector<std::pair<key, value>>;
    std::variant<std::nullptr_t, bool, std::int64_t, binary64, std::string, array, unsupported> data = nullptr;
    value() = default;
    template<class T> explicit value(T x) : data(std::move(x)) {}
};
struct limits final {
    std::size_t max_depth = 64;
    std::size_t max_nodes = 100000;
    std::size_t max_output_bytes = 8388608;
    std::size_t max_input_bytes = 16777216;
};
class error final : public std::runtime_error {
public:
    explicit error(const std::string& code) : std::runtime_error(code) {}
};
// GenericV1 semantics, including ordered mixed PHP keys and list reclassification.
// Each operation is independent. Errors contain a stable code and no input material.
std::string encode(const value& input, const limits& bounds = {});
std::string digest(const value& input, const limits& bounds = {});
// Strict tagged value bridge. Fixture expansion tags are intentionally test-only.
value from_tagged(const json::value& input);
limits limits_from_json(const json::value& input);
// Requests: {profile:"kumwe-canonical-json/generic-v1", operation:"encode"|"digest",
//           input:<tagged value>, limits?:{maxDepth,maxNodes,maxOutputBytes,maxInputBytes}}.
json::value evaluate(const json::value& request);
}
