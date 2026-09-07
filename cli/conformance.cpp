#include "support.hpp"
#include "decimal/decimal.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
namespace {
std::string unhex(std::string_view input) {
    if (input == "-") return {};
    test::require(input.size() % 2 == 0, "odd corpus hex");
    std::string result;
    for (std::size_t i = 0; i < input.size(); i += 2) {
        unsigned value = 0;
        for (unsigned j = 0; j < 2; ++j) {
            const auto c = input[i + j];
            test::require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), "invalid corpus hex");
            value = value * 16 + static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10);
        }
        result.push_back(static_cast<char>(value));
    }
    return result;
}
std::pair<unsigned, unsigned> inferred(std::string_view literal) {
    if (literal.starts_with('-')) literal.remove_prefix(1);
    const auto dot = literal.find('.');
    const auto integer = literal.substr(0, dot);
    const auto scale = dot == std::string_view::npos ? 0U : static_cast<unsigned>(literal.size() - dot - 1);
    const auto precision = std::max(1U, static_cast<unsigned>(integer == "0" ? 0 : integer.size()) + scale);
    return {precision, scale};
}
unsigned number(const std::string& input) {
    if (input == "-") return 0;
    const auto value = std::stoi(input);
    return value < 0 ? 255U : static_cast<unsigned>(value);
}
void replay(const std::vector<std::string>& fields) {
    test::require(fields.size() == 9, "corpus column count");
    const auto left = unhex(fields[2]);
    const auto right = unhex(fields[3]);
    const std::vector<std::string> operations = {"parse", "compare", "multiply", "round"};
    const std::vector<std::string> modes = {"half_up", "half_down", "half_even", "ceiling", "floor", "truncate"};
    const auto found = std::find(operations.begin(), operations.end(), fields[1]);
    test::require(found != operations.end(), "unknown corpus operation");
    const auto op = static_cast<unsigned>(found - operations.begin());
    auto [p, s] = inferred(left);
    auto [rp, rs] = inferred(right);
    unsigned mode = 0;
    if (op == 0) { p = number(fields[4]); s = number(fields[5]); rp = 0; rs = 0; }
    if (op == 3) {
        rp = number(fields[4]); rs = number(fields[5]);
        const auto rounding = std::find(modes.begin(), modes.end(), fields[6]);
        test::require(rounding != modes.end(), "unknown corpus rounding");
        mode = static_cast<unsigned>(rounding - modes.begin());
    }
    // Test the C++ implementation and the public C ABI against identical owner vectors.
    bool refused = false;
    std::string scalar;
    using kumwe::engine::decimal::value;
    try {
        const auto value_left = value::parse(left, p, s);
        if (op == 0) scalar = value_left.literal();
        if (op == 1) scalar = std::to_string(value_left.compare(value::parse(right, rp, rs)));
        if (op == 2) scalar = value_left.multiply(value::parse(right, rp, rs)).literal();
        if (op == 3) scalar = value_left.round(rp, rs, static_cast<kumwe::engine::decimal::rounding>(mode)).literal();
    } catch (const kumwe::engine::decimal::invalid_decimal&) { refused = true; }
    const bool expected_refusal = fields[7] == "invalid_argument";
    test::require(expected_refusal || fields[7] == "value", "unknown corpus outcome");
    test::require(refused == expected_refusal, "C++ corpus refusal mismatch");
    if (!refused) test::require(scalar == unhex(fields[8]), "C++ corpus bytes mismatch");
    auto request = test::request();
    test::row(request, op, p, s, rp, rs, mode, left, right);
    auto input = test::view(request);
    test::response output;
    const auto status = kumwe_engine_v1_decimal_batch(&input, &output.buffer);
    test::require(status == (expected_refusal ? 1U : 0U), "C ABI corpus refusal mismatch");
    if (expected_refusal) test::require(output.buffer == nullptr, "corpus refusal output");
    else test::require(output.bytes() == test::result(unhex(fields[8])), "C ABI corpus bytes mismatch");
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            std::string request = "KEC1"; test::integer(request, 1, 4); test::integer(request, 0, 4);
            auto input = test::view(request); test::response output;
            test::require(kumwe_engine_v1_capabilities(&input, &output.buffer) == 0, "capabilities");
            std::cout << output.bytes(); return 0;
        }
        test::require(argc == 2, "usage: kumwe-engine-conformance [decimal-v1.tsv]");
        std::ifstream input(argv[1]); test::require(input.good(), "cannot open corpus");
        std::string line; unsigned count = 0;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#' || line.starts_with("id\t")) continue;
            std::vector<std::string> fields;
            std::istringstream columns(line); std::string field;
            while (std::getline(columns, field, '\t')) fields.push_back(field);
            // std::getline omits an empty final field, which represents the empty hex literal.
            if (!line.empty() && line.back() == '\t') fields.emplace_back();
            try { replay(fields); }
            catch (...) { std::cerr << "Vector " << (fields.empty() ? "unknown" : fields[0]) << " failed\n"; throw; }
            ++count;
        }
        test::require(count != 0, "empty corpus");
        std::cout << "{\"vectors\":" << count << ",\"cpp\":\"passed\",\"c_abi\":\"passed\"}\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    catch (...) { std::cerr << "Unexpected native refusal\n"; return 1; }
}
