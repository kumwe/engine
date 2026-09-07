#include "support.hpp"
#include "decimal/decimal.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
using kumwe::engine::decimal::value;
using kumwe::engine::decimal::rounding;
namespace {
unsigned checks = 0;
void check(bool condition, std::string_view reason) { ++checks; test::require(condition, reason); }
void refuses(std::string_view literal, unsigned precision, unsigned scale) {
    bool failed = false;
    try { (void)value::parse(literal, precision, scale); }
    catch (const kumwe::engine::decimal::invalid_decimal&) { failed = true; }
    check(failed, "decimal refusal");
}
std::uint32_t invoke(std::string_view bytes, std::uint32_t expected) {
    auto input = test::view(bytes);
    test::response output;
    const auto status = kumwe_engine_v1_decimal_batch(&input, &output.buffer);
    check(status == expected, "ABI status");
    check((output.buffer == nullptr) == (status != 0), "atomic output ownership");
    return status;
}
void arithmetic() {
    check(value::parse("-0.000", 3, 3).literal() == "0.000", "zero retains scale");
    check(value::parse("-0." + std::string(65, '1'), 65, 65).literal().size() == 68, "largest negative fractional literal");
    check(value::parse("25000.00", 12, 2).multiply(value::parse("0.04938240", 12, 8)).literal() == "1234.5600000000", "money product");
    check(value::parse("99999999999999999999", 65, 0).multiply(value::parse("99999999999999999999", 65, 0)).literal() == "9999999999999999999800000000000000000001", "wide carry product");
    const std::array<std::string_view, 6> positive = {"1.24", "1.23", "1.24", "1.24", "1.23", "1.23"};
    const std::array<std::string_view, 6> negative = {"-1.24", "-1.23", "-1.24", "-1.23", "-1.24", "-1.23"};
    for (unsigned mode = 0; mode != 6; ++mode) {
        check(value::parse("1.2350", 8, 4).round(8, 2, static_cast<rounding>(mode)).literal() == positive[mode], "positive tie mode");
        check(value::parse("-1.2350", 8, 4).round(8, 2, static_cast<rounding>(mode)).literal() == negative[mode], "negative tie mode");
        check(value::parse("1.2300", 8, 4).round(8, 2, static_cast<rounding>(mode)).literal() == "1.23", "exact mode");
    }
    check(value::parse("1.225", 8, 3).round(8, 2, rounding::half_even).literal() == "1.22", "even tie");
    check(value::parse("1.23501", 8, 5).round(8, 2, rounding::half_down).literal() == "1.24", "beyond tie");
    check(value::parse("9.99", 8, 2).round(8, 1, rounding::half_up).literal() == "10.0", "rounding carry");
    check(value::parse("-0.004", 8, 3).round(8, 2, rounding::truncate).literal() == "0.00", "rounded negative zero");
    check(value::parse("1.23", 8, 2).round(8, 4, rounding::truncate).literal() == "1.2300", "scale widening");
    // Independent bounded integer oracle: exact identities, signs and ordering.
    for (std::int64_t left = -100; left <= 100; ++left) {
        const auto a = value::parse(std::to_string(left), 65, 0);
        for (std::int64_t right : {-101, -1, 0, 1, 99}) {
            const auto b = value::parse(std::to_string(right), 65, 0);
            check(a.multiply(b).literal() == std::to_string(left * right), "integer differential product");
            check(a.compare(b) == (left == right ? 0 : (left < right ? -1 : 1)), "integer differential compare");
            check(a.multiply(b).literal() == b.multiply(a).literal(), "commutative product");
        }
    }
}
void decimal_boundaries() {
    for (auto text : {"", "-", "+1", "01", "1.", ".5", "1e2", " 1", "1\n", "1,2", "1.2.3", "--1"}) refuses(text, 65, 4);
    refuses(std::string("1\0", 2), 65, 4);
    refuses("1", 0, 0); refuses("1", 66, 0); refuses("1", 1, 2);
    refuses("1", 1, 1); refuses("1.23", 2, 1); refuses(std::string(66, '9'), 65, 0);
    const std::array<int, 4> cases = {0, 1, 2, 3};
    for (int sample : cases) {
        bool failed = false;
        try {
            if (sample == 0) (void)value::parse("0." + std::string(33, '1'), 65, 33).multiply(value::parse("0." + std::string(33, '1'), 65, 33));
            if (sample == 1) (void)value::parse(std::string(33, '9'), 65, 0).multiply(value::parse(std::string(33, '9'), 65, 0));
            if (sample == 2) (void)value::parse("9.99", 3, 2).round(2, 1, rounding::half_up);
            if (sample == 3) (void)value::parse("1", 1, 0).compare(value::parse("1.0", 2, 1));
        } catch (const kumwe::engine::decimal::invalid_decimal&) { failed = true; }
        check(failed, "arithmetic boundary");
    }
}
void abi_boundaries() {
    auto bytes = test::request();
    test::row(bytes, 2, 10, 2, 10, 2, 0, "1.25", "2.00");
    auto input = test::view(bytes);
    test::response output;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 0, "valid batch");
    check(output.bytes() == test::result("2.5000"), "golden wire output");
    const auto* existing_owner = output.buffer;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 1, "nonempty output slot refused");
    check(output.buffer == existing_owner && output.bytes() == test::result("2.5000"), "preexisting owner preserved");
    kumwe_engine_v1_buffer_release(&output.buffer);
    kumwe_engine_v1_buffer_release(&output.buffer);
    check(output.buffer == nullptr, "consumed owner");
    for (std::size_t size = 0; size < bytes.size(); ++size) invoke(std::string_view(bytes).substr(0, size), 1);
    invoke(bytes + "x", 1);
    auto unsupported = bytes; unsupported[3] = '2'; invoke(unsupported, 2);
    auto invalid_op = bytes; invalid_op[20] = 4; invoke(invalid_op, 1);
    auto reserved = bytes; reserved[25] = 1; invoke(reserved, 1);
    for (auto limit : {0U, 1U, 7U, 8U, 15U, 1048577U}) {
        auto limited = test::request(1, limit); test::row(limited, 0, 10, 2, 0, 0, 0, "1.25");
        invoke(limited, 6);
    }
    for (auto budget : {0ULL, 1ULL, 511ULL, 1000000001ULL}) {
        auto limited = test::request(1, 1048576, budget); test::row(limited, 0, 10, 2, 0, 0, 0, "1.25");
        invoke(limited, 6);
    }
    auto exact = test::request(1, 16, 512); test::row(exact, 0, 10, 2, 0, 0, 0, "1.25");
    invoke(exact, 0);
    auto padded = test::request(1, 1048576, 4867); test::row(padded, 2, 65, 32, 65, 33, 0, "1", "0");
    invoke(padded, 6);
    padded = test::request(1, 1048576, 4868); test::row(padded, 2, 65, 32, 65, 33, 0, "1", "0");
    invoke(padded, 0);
    auto batch = test::request(2); test::row(batch, 0, 10, 2, 0, 0, 0, "1.25"); test::row(batch, 0, 10, 2, 0, 0, 0, "bad");
    invoke(batch, 1);
    check(kumwe_engine_v1_decimal_batch(nullptr, &output.buffer) == 1, "null request");
    check(kumwe_engine_v1_decimal_batch(&input, nullptr) == 1, "null output");
    input.struct_size = 4;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 1, "short struct");
    input.struct_size = 4097;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 1, "large struct");
    input.struct_size = 32;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 0, "additive struct accepted");
    kumwe_engine_v1_buffer_release(&output.buffer);
    input.struct_size = 24; input.abi_major = 2;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 2, "wrong ABI");
    input.abi_major = 1; input.data = nullptr; input.size = 1;
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 1, "null bytes");
    input.size = std::numeric_limits<std::uint64_t>::max();
    check(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 6, "overflow bytes refused before access");
    kumwe_engine_v1_view empty{24,1,nullptr,0};
    check(kumwe_engine_v1_buffer_view(nullptr, &empty) == 1, "null handle view");
    check(kumwe_engine_v1_buffer_view(nullptr, nullptr) == 1, "null view output");
    auto caps = std::string("KEC1"); test::integer(caps, 1, 4); test::integer(caps, 1, 4);
    input = test::view(caps);
    check(kumwe_engine_v1_capabilities(&input, &output.buffer) == 0, "capability handshake");
    check(output.bytes().find("unstable-development") != std::string::npos, "development identity");
    kumwe_engine_v1_buffer_release(&output.buffer);
    caps[8] = 2;
    check(kumwe_engine_v1_capabilities(&input, &output.buffer) == 3, "unknown capability");
    caps[8] = 1; caps[4] = 2;
    check(kumwe_engine_v1_capabilities(&input, &output.buffer) == 2, "unknown requested ABI");
}
void lifecycle() {
    auto bytes = test::request(); test::row(bytes, 0, 4, 2, 0, 0, 0, "1.25");
    const auto input = test::view(bytes);
    for (unsigned cycle = 0; cycle != 10000; ++cycle) {
        test::response output;
        test::require(kumwe_engine_v1_decimal_batch(&input, &output.buffer) == 0, "lifecycle create");
        test::require(output.bytes() == test::result("1.25"), "lifecycle read");
    }
    test::response shared;
    check(kumwe_engine_v1_decimal_batch(&input, &shared.buffer) == 0, "shared immutable buffer");
    std::vector<std::thread> readers;
    for (unsigned thread = 0; thread != 4; ++thread) readers.emplace_back([&] {
        for (unsigned round = 0; round != 1000; ++round) {
            test::require(shared.bytes() == test::result("1.25"), "concurrent read");
            test::response independent;
            test::require(kumwe_engine_v1_decimal_batch(&input, &independent.buffer) == 0, "reentrant operation");
        }
    });
    for (auto& reader : readers) reader.join();
}
}
int main() {
    try { arithmetic(); decimal_boundaries(); abi_boundaries(); lifecycle(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    catch (...) { std::cerr << "Unexpected refusal\n"; return 1; }
    std::cout << checks << " behavior/boundary assertions; 10000 lifecycle cycles; 4 concurrent readers passed\n";
}
