#include <kumwe/engine/engine.h>
#include "batch.hpp"
#include "decimal/decimal.hpp"
#include "plan/plan.hpp"
#include "vm/error.hpp"
#include "canonical/canonical.hpp"
#include "build_identity.hpp"
#include <atomic>
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
struct kumwe_engine_v1_plan final {
    explicit kumwe_engine_v1_plan(kumwe::engine::execution::plan value) : compiled(std::move(value)) {}
    const kumwe::engine::execution::plan compiled;
};
struct kumwe_engine_v1_cancellation final { std::atomic<bool> requested{false}; };
namespace {
std::string_view request_bytes(const kumwe_engine_v1_view* view, std::size_t maximum = 1048576) {
    if (view == nullptr || view->struct_size < sizeof(kumwe_engine_v1_view) || view->struct_size > 4096)
        throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    if (view->abi_major != 1) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
    if (view->size > maximum) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    if (view->data == nullptr) {
        if (view->size != 0) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        return {};
    }
    return {reinterpret_cast<const char*>(view->data), static_cast<std::size_t>(view->size)};
}
template <typename Operation>
kumwe_engine_v1_status allocated(kumwe_engine_v1_buffer** response, Operation operation) noexcept {
    if (response == nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    // The caller owns an initially empty output slot; no existing handle is accepted.
    if (*response != nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    try {
        *response = new kumwe_engine_v1_buffer(operation());
        return KUMWE_ENGINE_V1_OK;
    } catch (const kumwe::engine::refusal& error) { return error.code; }
    catch (const kumwe::engine::decimal::invalid_decimal&) { return KUMWE_ENGINE_V1_INVALID_INPUT; }
    catch (const kumwe::engine::vm::error&) { return KUMWE_ENGINE_V1_INVALID_INPUT; }
    catch (const std::invalid_argument&) { return KUMWE_ENGINE_V1_INVALID_INPUT; }
    catch (const std::bad_alloc&) { return KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
    catch (...) { return KUMWE_ENGINE_V1_INTERNAL_FAILURE; }
}
template <typename Operation>
kumwe_engine_v1_status guarded(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response, Operation operation) noexcept {
    return allocated(response, [&]() { return operation(request_bytes(request)); });
}
}
extern "C" {
kumwe_engine_v1_status kumwe_engine_v1_capabilities(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response) {
    return guarded(request, response, kumwe::engine::capabilities);
}
kumwe_engine_v1_status kumwe_engine_v1_decimal_batch(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response) {
    return guarded(request, response, kumwe::engine::decimal_batch);
}
kumwe_engine_v1_status kumwe_engine_v1_canonical(const kumwe_engine_v1_view* request, kumwe_engine_v1_buffer** response) {
    return allocated(response, [&]() {
        using value = kumwe::engine::json::value;
        using namespace kumwe::engine;
        auto input = json::parse(request_bytes(request, 67108864), 67108864, 2000000, 512);
        if (!input.is<value::object>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        if (input.at("wire_version") != value(std::int64_t{1})) throw refusal(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
        if (input.at("corpus_digest") != value(KUMWE_ENGINE_CANONICAL_CORPUS)) throw refusal(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        auto& object = std::get<value::object>(input.data);
        object.erase("wire_version"); object.erase("corpus_digest");
        try { return json::encode(canonical::evaluate(input), 67108864); }
        catch (const canonical::error& error) {
            return json::encode(value(value::object{{"finding", value(error.what())}}));
        }
    });
}
kumwe_engine_v1_status kumwe_engine_v1_compile(const kumwe_engine_v1_view* request, kumwe_engine_v1_plan** response) {
    if (response == nullptr || *response != nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    try {
        auto compiled = kumwe::engine::execution::plan::compile(request_bytes(request, 67108864));
        *response = new kumwe_engine_v1_plan(std::move(compiled));
        return KUMWE_ENGINE_V1_OK;
    } catch (const kumwe::engine::refusal& error) { return error.code; }
    catch (const kumwe::engine::vm::error&) { return KUMWE_ENGINE_V1_INVALID_PROGRAM; }
    catch (const std::bad_alloc&) { return KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
    catch (...) { return KUMWE_ENGINE_V1_INTERNAL_FAILURE; }
}
kumwe_engine_v1_status kumwe_engine_v1_execute(const kumwe_engine_v1_plan* plan,
    const kumwe_engine_v1_view* request, const kumwe_engine_v1_cancellation* cancellation, kumwe_engine_v1_buffer** response) {
    return allocated(response, [&]() {
        if (plan == nullptr) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        return plan->compiled.execute(request_bytes(request, 67108864), cancellation == nullptr ? nullptr : &cancellation->requested);
    });
}
kumwe_engine_v1_status kumwe_engine_v1_plan_describe(const kumwe_engine_v1_plan* plan, kumwe_engine_v1_buffer** response) {
    return allocated(response, [&]() {
        if (plan == nullptr) throw kumwe::engine::refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        return plan->compiled.describe();
    });
}
void kumwe_engine_v1_plan_release(kumwe_engine_v1_plan** owner) {
    if (owner == nullptr) return;
    const auto* plan = *owner;
    *owner = nullptr;
    delete plan;
}
kumwe_engine_v1_status kumwe_engine_v1_cancellation_create(kumwe_engine_v1_cancellation** response) {
    if (response == nullptr || *response != nullptr) return KUMWE_ENGINE_V1_INVALID_INPUT;
    try { *response = new kumwe_engine_v1_cancellation(); return KUMWE_ENGINE_V1_OK; }
    catch (const std::bad_alloc&) { return KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
    catch (...) { return KUMWE_ENGINE_V1_INTERNAL_FAILURE; }
}
void kumwe_engine_v1_cancellation_request(kumwe_engine_v1_cancellation* cancellation) {
    if (cancellation != nullptr) cancellation->requested.store(true, std::memory_order_relaxed);
}
void kumwe_engine_v1_cancellation_release(kumwe_engine_v1_cancellation** owner) {
    if (owner == nullptr) return;
    const auto* cancellation = *owner;
    *owner = nullptr;
    delete cancellation;
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
