#include <kumwe/engine/engine.h>
#include "batch.hpp"
#include "decimal/decimal.hpp"
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <utility>
static_assert(sizeof(void*) == 8, "The draft ABI supports 64-bit platforms only");
static_assert(sizeof(kumwe_engine_v1_view) == 24);
static_assert(offsetof(kumwe_engine_v1_view, data) == 8);
static_assert(offsetof(kumwe_engine_v1_view, size) == 16);
struct kumwe_engine_v1_buffer final {
    explicit kumwe_engine_v1_buffer(std::string data) : bytes(std::move(data)) {}
    const std::string bytes;
};
namespace {
std::string_view request_bytes(const kumwe_engine_v1_view* view) {
    if (view == nullptr || view->struct_size < sizeof(kumwe_engine_v1_view) || view->struct_size > 4096)
        throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    if (view->abi_major != 1) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
    if (view->size > 1048576) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    if (view->data == nullptr) {
        if (view->size != 0) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        return {};
    }
    return {reinterpret_cast<const char*>(view->data), static_cast<std::size_t>(view->size)};
}
template <typename Operation>
kumwe_engine_v1_status guarded(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response, Operation operation) noexcept {
    if (response == nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    // The caller owns an initially empty output slot; no existing handle is accepted.
    if (*response != nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    try {
        *response = new kumwe_engine_v1_buffer(operation(request_bytes(request)));
        return KUMWE_ENGINE_V1_OK;
    } catch (const kumwe::engine::refusal& error) { return error.code; }
    catch (const kumwe::engine::decimal::invalid_decimal&) { return KUMWE_ENGINE_V1_INVALID_INPUT; }
    catch (const std::bad_alloc&) { return KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
    catch (...) { return KUMWE_ENGINE_V1_INTERNAL_FAILURE; }
}
}
extern "C" {
kumwe_engine_v1_status kumwe_engine_v1_capabilities(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response) {
    return guarded(request, response, kumwe::engine::capabilities);
}
kumwe_engine_v1_status kumwe_engine_v1_decimal_batch(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response) {
    return guarded(request, response, kumwe::engine::decimal_batch);
}
kumwe_engine_v1_status kumwe_engine_v1_buffer_view(const kumwe_engine_v1_buffer* buffer, kumwe_engine_v1_view* output) {
    if (output == nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    // Only the first eight bytes may be accessed until the advertised struct size is checked.
    if (output->struct_size < sizeof(kumwe_engine_v1_view) || output->struct_size > 4096)
        return KUMWE_ENGINE_V1_INVALID_INPUT;
    output->data = nullptr;
    output->size = 0;
    if (output->abi_major != 1) return KUMWE_ENGINE_V1_UNSUPPORTED_VERSION;
    if (buffer == nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    output->data = reinterpret_cast<const std::uint8_t*>(buffer->bytes.data());
    output->size = buffer->bytes.size();
    return KUMWE_ENGINE_V1_OK;
}
void kumwe_engine_v1_buffer_release(kumwe_engine_v1_buffer** owner) {
    if (owner == nullptr) return;
    auto* buffer = *owner;
    *owner = nullptr;
    delete buffer;
}
}
