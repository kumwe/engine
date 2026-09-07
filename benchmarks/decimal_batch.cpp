#include "support.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
int main() {
    using clock = std::chrono::steady_clock;
    for (unsigned rows : {1U, 32U, 256U, 4096U}) {
        auto bytes = test::request(rows);
        for (unsigned row = 0; row < rows; ++row) test::row(bytes, 2, 12, 2, 12, 8, 0, "25000.00", "0.04938240");
        std::vector<std::int64_t> durations;
        for (unsigned run = 0; run < 120; ++run) {
            const auto start = clock::now();
            { // Include request copy, C ABI invocation, result copy, and matching release.
                const auto request = bytes;
                const auto input = test::view(request);
                test::response output;
                test::require(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 0, "benchmark refusal");
                const auto copy = output.bytes();
                test::require(!copy.empty(), "benchmark result");
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count();
            if (run >= 20) durations.push_back(elapsed);
        }
        std::sort(durations.begin(), durations.end());
        std::cout << "{\"rows\":" << rows << ",\"request_bytes\":" << bytes.size()
            << ",\"samples\":100,\"warmup\":20,\"p50_ns\":" << durations[49]
            << ",\"p95_ns\":" << durations[94] << ",\"p99_ns\":" << durations[98] << "}\n";
    }
}
