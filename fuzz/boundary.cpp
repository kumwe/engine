#include <kumwe/engine/engine.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "../src/value/json.hpp"
#include "../src/batch.hpp"
namespace {
struct fixed_plan final {
    kumwe_engine_v1_plan* handle = nullptr;
    fixed_plan() {
        constexpr const char* source = R"({"wire_version":1,"profile":"formula-draft/1","corpus_digest":"11033679b018fdc9a192e954ef11089444a00a1d89c6279d3c192be9252cf42f","program":{"op":"literal","type":"integer","value":1}})";
        const kumwe_engine_v1_view input{sizeof(kumwe_engine_v1_view), 1,
            reinterpret_cast<const std::uint8_t*>(source), std::strlen(source)};
        if (kumwe_engine_v1_compile(&input, &handle) != 0) std::abort();
    }
    ~fixed_plan() { kumwe_engine_v1_plan_release(&handle); }
};
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // Exercise the fused scanner on arbitrary bytes even when mutation cannot
    // enter a valid compiled envelope. Admission and exact limits must match
    // the materializing writer; successful quotes use its independent bytes.
    if (size <= 1048577) {
        using namespace kumwe::engine;
        const json::value text(size == 0 ? std::string{} : std::string(reinterpret_cast<const char*>(data), size));
        for (const auto limit : {std::size_t{0}, size, size * 6 + 2}) {
            std::uint32_t written_status = 0, counted_status = 0, measured_status = 0;
            std::string written;
            std::size_t counted = 0;
            json::encoded_value measured;
            try { written = json::encode(text, limit); } catch (const refusal& e) { written_status = e.code; }
            try { counted = json::encoded_size(text, limit); } catch (const refusal& e) { counted_status = e.code; }
            try { measured = json::encode_with_quoted_size(text, limit); } catch (const refusal& e) { measured_status = e.code; }
            if (written_status != counted_status || written_status != measured_status) std::abort();
            if (written_status == 0 && (written.size() != counted || written != measured.bytes
                || measured.quoted_size != json::encode(json::value(written)).size())) std::abort();
        }
    }
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
    static const fixed_plan plan;
    const auto execution_status = kumwe_engine_v1_execute(plan.handle, &input, nullptr, &buffer);
    if ((execution_status == 0) != (buffer != nullptr)) std::abort();
    if (buffer != nullptr) {
        kumwe_engine_v1_view output{sizeof(kumwe_engine_v1_view), 1, nullptr, 0};
        if (kumwe_engine_v1_buffer_view(buffer, &output) != 0 || output.size > 67108864) std::abort();
    }
    kumwe_engine_v1_buffer_release(&buffer);
    const auto capability_status = kumwe_engine_v1_capabilities(&input, &buffer);
    if ((capability_status == 0) != (buffer != nullptr)) std::abort();
    kumwe_engine_v1_buffer_release(&buffer);
    const auto canonical_status = kumwe_engine_v1_canonical(&input, &buffer);
    if ((canonical_status == 0) != (buffer != nullptr)) std::abort();
    if (buffer != nullptr) {
        kumwe_engine_v1_view output{sizeof(kumwe_engine_v1_view), 1, nullptr, 0};
        if (kumwe_engine_v1_buffer_view(buffer, &output) != 0 || output.size > 67108864) std::abort();
    }
    kumwe_engine_v1_buffer_release(&buffer);
    kumwe_engine_v1_buffer_release(&buffer);
    return 0;
}
