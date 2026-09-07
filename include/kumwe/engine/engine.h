#ifndef KUMWE_ENGINE_ENGINE_H
#define KUMWE_ENGINE_ENGINE_H
#include <stdint.h>
#if defined(__GNUC__) || defined(__clang__)
#define KUMWE_ENGINE_API __attribute__((visibility("default")))
#else
#define KUMWE_ENGINE_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Draft ABI: see docs/abi.md for ownership, bounds, framing and compatibility. */
typedef struct kumwe_engine_v1_view {
    uint32_t struct_size;
    uint32_t abi_major;
    const uint8_t *data;
    uint64_t size;
} kumwe_engine_v1_view;
typedef struct kumwe_engine_v1_buffer kumwe_engine_v1_buffer;
typedef uint32_t kumwe_engine_v1_status;
#define KUMWE_ENGINE_V1_OK UINT32_C(0)
#define KUMWE_ENGINE_V1_INVALID_INPUT UINT32_C(1)
#define KUMWE_ENGINE_V1_UNSUPPORTED_VERSION UINT32_C(2)
#define KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY UINT32_C(3)
#define KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS UINT32_C(4)
#define KUMWE_ENGINE_V1_INVALID_PROGRAM UINT32_C(5)
#define KUMWE_ENGINE_V1_EXHAUSTED_LIMIT UINT32_C(6)
#define KUMWE_ENGINE_V1_CANCELLED UINT32_C(7)
#define KUMWE_ENGINE_V1_INTERNAL_FAILURE UINT32_C(8)
/* Output slots must initially be null; refusals then leave *response null.
 * A nonempty slot is refused unchanged so its existing owner remains releasable. */
KUMWE_ENGINE_API kumwe_engine_v1_status kumwe_engine_v1_capabilities(
    const kumwe_engine_v1_view *request, kumwe_engine_v1_buffer **response);
KUMWE_ENGINE_API kumwe_engine_v1_status kumwe_engine_v1_decimal_batch(
    const kumwe_engine_v1_view *request, kumwe_engine_v1_buffer **response);
/* Caller initializes output struct_size and abi_major. For supported struct sizes,
 * failure clears data and size. An invalid size is refused without accessing them. */
KUMWE_ENGINE_API kumwe_engine_v1_status kumwe_engine_v1_buffer_view(
    const kumwe_engine_v1_buffer *buffer, kumwe_engine_v1_view *output);
/* Null owner or null *owner is harmless. Valid unique ownership is required. */
KUMWE_ENGINE_API void kumwe_engine_v1_buffer_release(kumwe_engine_v1_buffer **owner);
#ifdef __cplusplus
}
#endif
#endif
