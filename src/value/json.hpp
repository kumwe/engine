#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace kumwe::engine::json {
// A transport number outside int64 is retained losslessly, never coerced to binary64.
struct number final { std::string spelling; bool operator==(const number&) const = default; };
struct value final {
    using list = std::vector<value>;
    using object = std::map<std::string, value, std::less<>>;
    std::variant<std::nullptr_t, bool, std::int64_t, std::string, list, object, number> data = nullptr;
    value() = default;
    value(std::nullptr_t) : data(nullptr) {}
    value(bool x) : data(x) {}
    value(std::int64_t x) : data(x) {}
    value(std::string x) : data(std::move(x)) {}
    value(const char* x) : data(std::string(x)) {}
    value(list x) : data(std::move(x)) {}
    value(object x) : data(std::move(x)) {}
    value(number x) : data(std::move(x)) {}
    template<class T> bool is() const noexcept { return std::holds_alternative<T>(data); }
    template<class T> const T& as() const { return std::get<T>(data); }
    const value* find(std::string_view key) const noexcept;
    const value& at(std::string_view key) const;
    bool operator==(const value&) const = default;
};
// Strict UTF-8, duplicate-key rejection, exact int64 parsing; no ambient locale.
value parse(std::string_view source, std::size_t max_bytes = 67108864,
            std::size_t max_nodes = 200000, std::size_t max_depth = 128);
std::string encode(const value& source, std::size_t max_bytes = 67108864);
// Same traversal, escaping, UTF-8 admission and byte limit, without materializing output.
std::size_t encoded_size(const value& source, std::size_t max_bytes = 67108864);
bool valid_utf8(std::string_view source) noexcept;
}
