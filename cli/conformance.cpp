#include "support.hpp"
#include "decimal/decimal.hpp"
#include "canonical/sha256.hpp"
#include "value/json.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
namespace {
constexpr std::size_t maximum_input_bytes = 67108864;
struct plan_owner final {
    kumwe_engine_v1_plan* handle = nullptr;
    ~plan_owner() { kumwe_engine_v1_plan_release(&handle); }
};
std::string read_input(const char* path) {
    std::ifstream file;
    std::istream* stream = &std::cin;
    if (std::string_view(path) != "-") {
        file.open(path, std::ios::binary);
        test::require(file.is_open(), "cannot open input");
        stream = &file;
    }
    std::string bytes;
    char chunk[8192];
    while (stream->read(chunk, sizeof(chunk)) || stream->gcount() > 0) {
        const auto count = static_cast<std::size_t>(stream->gcount());
        test::require(count <= maximum_input_bytes - bytes.size(), "input exceeds diagnostic byte limit");
        bytes.append(chunk, count);
    }
    test::require(!stream->bad() && stream->eof(), "cannot read input");
    return bytes;
}
int emit(kumwe_engine_v1_status status, const test::response& output) {
    if (status == KUMWE_ENGINE_V1_OK) std::cout << output.bytes() << '\n';
    else std::cout << "{\"status\":" << status << "}\n";
    test::require(std::cout.good(), "cannot write output");
    return static_cast<int>(status);
}
kumwe_engine_v1_status capabilities(test::response& output) {
    std::string request = "KEC1"; test::integer(request, 1, 4); test::integer(request, 0, 4);
    const auto input = test::view(request);
    return kumwe_engine_v1_capabilities(&input, &output.buffer);
}
int verify_bundle(const char* path) {
    using namespace kumwe::engine;
    using value = json::value;
    test::response output;
    const auto status = capabilities(output);
    if (status != KUMWE_ENGINE_V1_OK) return emit(status, output);
    const auto manifest = json::parse(output.bytes());
    const std::vector<std::pair<std::string, std::string>> corpora = {
        {"decimal/decimal-v1.tsv", manifest.at("corpus_sha256").as<std::string>()},
        {"definition/formula-v1.json", "formula-draft/1"},
        {"document/validation-v1.json", "normalized-document-draft/1"},
        {"reporting/materialization-v1.json", "report-materialization-draft/1"},
        {"canonical/generic-v1.json", "kumwe-canonical-json/generic-v1"}
    };
    value::list checked;
    for (const auto& [relative, profile] : corpora) {
        std::string expected = profile;
        if (!relative.starts_with("decimal/")) {
            expected.clear();
            for (const auto& contract : manifest.at("computation").at("contracts").as<value::list>()) {
                if (contract.at("profile").as<std::string>() == profile) expected = contract.at("corpus_digest").as<std::string>();
            }
            test::require(!expected.empty(), "missing runtime corpus identity");
        }
        const auto file = std::string(path) + "/" + relative;
        const auto bytes = read_input(file.c_str());
        canonical::sha256 digest; digest.update(bytes);
        if (digest.finish() != expected) {
            std::cout << "{\"status\":4}\n";
            return static_cast<int>(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        }
        checked.emplace_back(value::object{{"path", value(relative)}, {"sha256", value(expected)}});
    }
    std::cout << json::encode(value(value::object{{"corpora", value(std::move(checked))},
        {"status", value(std::int64_t{0})}})) << '\n';
    test::require(std::cout.good(), "cannot write output");
    return 0;
}
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
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--capabilities")) {
            test::response output;
            return emit(capabilities(output), output);
        }
        const std::string_view operation = argv[1];
        if (argc == 2 && operation == "--help") {
            std::cout << "kumwe-engine-conformance [decimal-v1.tsv | --capabilities | --verify-bundle corpus-directory | --compile request.json | --execute compile.json batch.json | --canonical request.json]\n"
                         "Use - for one JSON input read from standard input. Native refusals emit a JSON status and the same exit code.\n";
            return 0;
        }
        if (argc == 3 && operation == "--verify-bundle") return verify_bundle(argv[2]);
        if (argc == 3 && operation == "--canonical") {
            const auto bytes = read_input(argv[2]); const auto input = test::view(bytes); test::response output;
            return emit(kumwe_engine_v1_canonical(&input, &output.buffer), output);
        }
        if ((argc == 3 && operation == "--compile") || (argc == 4 && operation == "--execute")) {
            test::require(argc != 4 || std::string_view(argv[2]) != "-" || std::string_view(argv[3]) != "-",
                "only one input can use standard input");
            const auto bytes = read_input(argv[2]); const auto input = test::view(bytes); plan_owner plan;
            auto status = kumwe_engine_v1_compile(&input, &plan.handle);
            test::response output;
            if (status != KUMWE_ENGINE_V1_OK) return emit(status, output);
            if (operation == "--compile") status = kumwe_engine_v1_plan_describe(plan.handle, &output.buffer);
            else {
                const auto batch = read_input(argv[3]); const auto batch_input = test::view(batch);
                status = kumwe_engine_v1_execute(plan.handle, &batch_input, nullptr, &output.buffer);
            }
            return emit(status, output);
        }
        test::require(argc == 2 && !operation.starts_with("--"), "invalid arguments; use --help");
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
