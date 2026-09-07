#pragma once
#include <kumwe/engine/engine.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
namespace test {
inline void require(bool condition, std::string_view reason) {
    if (!condition) throw std::runtime_error(std::string(reason));
}
inline void integer(std::string& bytes, std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) bytes.push_back(static_cast<char>((value >> (8U * i)) & 255));
}
inline std::string request(std::uint32_t count = 1, std::uint32_t limit = 1048576, std::uint64_t budget = 1000000000) {
    std::string result = "KED1";
    integer(result, count, 4); integer(result, limit, 4); integer(result, budget, 8);
    return result;
}
inline void row(std::string& result, unsigned op, unsigned p, unsigned s, unsigned rp, unsigned rs, unsigned mode, std::string_view left, std::string_view right = {}) {
    for (unsigned item : {op, p, s, rp, rs, mode}) integer(result, item, 1);
    integer(result, left.size(), 2); integer(result, right.size(), 2);
    result += left; result += right;
}
inline kumwe_engine_v1_view view(std::string_view bytes) {
    return {sizeof(kumwe_engine_v1_view), 1, reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}
struct response final {
    kumwe_engine_v1_buffer* buffer = nullptr;
    response() = default;
    response(const response&) = delete;
    response& operator=(const response&) = delete;
    ~response() { kumwe_engine_v1_buffer_release(&buffer); }
    std::string bytes() const {
        kumwe_engine_v1_view output = {sizeof(kumwe_engine_v1_view), 1, nullptr, 0};
        require(kumwe_engine_v1_buffer_view(buffer, &output) == 0, "buffer view");
        return {reinterpret_cast<const char*>(output.data), static_cast<std::size_t>(output.size)};
    }
};
inline std::string result(std::string_view scalar) {
    std::string expected = "KER1";
    integer(expected, 1, 4); integer(expected, scalar.size(), 4); expected += scalar;
    return expected;
}
}
