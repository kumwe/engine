#include "batch.hpp"
#include "decimal/decimal.hpp"
#include "build_identity.hpp"
#include <kumwe/engine/engine.h>
#include <limits>
namespace kumwe::engine {
namespace {
class reader final {
public:
    explicit reader(std::string_view bytes) : bytes_(bytes) {}
    std::string_view take(std::size_t size) {
        if (size > bytes_.size()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        const auto result = bytes_.substr(0, size);
        bytes_.remove_prefix(size);
        return result;
    }
    std::uint64_t integer(unsigned width) {
        auto bytes = take(width);
        std::uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[i])) << (8U * i);
        return result;
    }
    bool empty() const noexcept { return bytes_.empty(); }
private:
    std::string_view bytes_;
};
void append_u32(std::string& result, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) result.push_back(static_cast<char>((value >> (8U * i)) & 255));
}
}
std::string capabilities(std::string_view request) {
    reader input(request);
    if (input.take(4) != "KEC1") throw refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
    if (input.integer(4) != 1) throw refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
    if ((input.integer(4) & ~UINT64_C(1)) != 0) throw refusal(KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY);
    if (!input.empty()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    return KUMWE_ENGINE_CAPABILITIES_JSON;
}
std::string decimal_batch(std::string_view request) {
    if (request.size() > 1048576) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    reader input(request);
    if (input.take(4) != "KED1") throw refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
    const auto count = input.integer(4);
    const auto output_limit = input.integer(4);
    auto budget = input.integer(8);
    if (count == 0 || count > 4096 || output_limit == 0 || output_limit > 1048576 ||
        budget == 0 || budget > 1000000000) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    if (output_limit < 8) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    std::string output = "KER1";
    append_u32(output, static_cast<std::uint32_t>(count));
    for (std::uint64_t row = 0; row < count; ++row) {
        const auto operation = input.integer(1);
        const auto precision = static_cast<unsigned>(input.integer(1));
        const auto scale = static_cast<unsigned>(input.integer(1));
        const auto target_precision = static_cast<unsigned>(input.integer(1));
        const auto target_scale = static_cast<unsigned>(input.integer(1));
        const auto mode = input.integer(1);
        const auto left_size = input.integer(2);
        const auto right_size = input.integer(2);
        if (left_size > 68 || right_size > 68) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        const auto cost = 512 + left_size * right_size;
        if (cost > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        budget -= cost;
        const auto left_literal = input.take(static_cast<std::size_t>(left_size));
        const auto right_literal = input.take(static_cast<std::size_t>(right_size));
        if (operation > 3 || (operation != 3 && mode != 0) ||
            (operation == 0 && (target_precision != 0 || target_scale != 0 || right_size != 0)) ||
            (operation == 3 && (right_size != 0 || mode > 5))) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        const auto left = decimal::value::parse(left_literal, precision, scale);
        std::string result;
        switch (operation) {
        case 0: result = left.literal(); break;
        case 1: result = std::to_string(left.compare(decimal::value::parse(right_literal, target_precision, target_scale))); break;
        case 2: result = left.multiply(decimal::value::parse(right_literal, target_precision, target_scale)).literal(); break;
        case 3: result = left.round(target_precision, target_scale, static_cast<decimal::rounding>(mode)).literal(); break;
        default: throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        }
        if (result.size() + 4 > output_limit - output.size()) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        append_u32(output, static_cast<std::uint32_t>(result.size()));
        output += result;
    }
    if (!input.empty()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    return output;
}
}
