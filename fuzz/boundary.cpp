#include <kumwe/engine/engine.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    kumwe_engine_v1_view input{sizeof(kumwe_engine_v1_view), 1, data, size};
    kumwe_engine_v1_buffer* buffer = nullptr;
    const auto status = kumwe_engine_v1_decimal_batch(&input, &buffer);
    if ((status == 0) != (buffer != nullptr)) std::abort();
    if (buffer != nullptr) {
        kumwe_engine_v1_view output{sizeof(kumwe_engine_v1_view), 1, nullptr, 0};
        if (kumwe_engine_v1_buffer_view(buffer, &output) != 0 || output.size > 1048576) std::abort();
    }
    kumwe_engine_v1_buffer_release(&buffer);
    kumwe_engine_v1_buffer_release(&buffer);
    const auto capability_status = kumwe_engine_v1_capabilities(&input, &buffer);
    if ((capability_status == 0) != (buffer != nullptr)) std::abort();
    kumwe_engine_v1_buffer_release(&buffer);
    return 0;
}
