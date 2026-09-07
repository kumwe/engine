#include "support.hpp"
#include "value/json.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
using value = kumwe::engine::json::value;
struct plan_owner final {
    kumwe_engine_v1_plan* handle = nullptr;
    ~plan_owner() { kumwe_engine_v1_plan_release(&handle); }
};
value parse(std::string_view input) { return kumwe::engine::json::parse(input); }
std::string json_bytes(const value& input) { return kumwe::engine::json::encode(input); }
std::string digest(std::string_view profile) {
    std::string bytes = "KEC1"; test::integer(bytes, 1, 4); test::integer(bytes, 0, 4);
    const auto input = test::view(bytes); test::response result;
    test::require(kumwe_engine_v1_capabilities(&input, &result.buffer) == 0, "benchmark capabilities");
    const auto metadata = parse(result.bytes());
    for (const auto& contract : metadata.at("computation").at("contracts").as<value::list>())
        if (contract.at("profile").as<std::string>() == profile) return contract.at("corpus_digest").as<std::string>();
    throw std::runtime_error("benchmark profile unavailable");
}
void compile(std::string_view source, plan_owner& result) {
    const auto input = test::view(source);
    test::require(kumwe_engine_v1_compile(&input, &result.handle) == 0, "benchmark compile");
}
std::size_t execute(const kumwe_engine_v1_plan* plan, std::string_view request) {
    // Include both boundary copies and the matching native release in measured time.
    const std::string copied(request); const auto input = test::view(copied); test::response result;
    const auto status = kumwe_engine_v1_execute(plan, &input, nullptr, &result.buffer);
    test::require(status == 0, "benchmark execute status " + std::to_string(status));
    const auto output = result.bytes();
    test::require(output.find("\"results\":[{") != std::string::npos, "benchmark result missing");
    return output.size();
}
template<class Run>
void measure(std::string_view profile, std::string_view phase, unsigned rows,
    std::size_t request_bytes, Run run) {
    using clock = std::chrono::steady_clock;
    std::vector<std::int64_t> durations;
    std::size_t output_bytes = 0;
    for (unsigned sample = 0; sample < 120; ++sample) {
        const auto start = clock::now();
        std::size_t size;
        try { size = run(); }
        catch (const std::exception& error) {
            throw std::runtime_error(std::string(profile) + "/" + std::string(phase) + "/"
                + std::to_string(rows) + ": " + error.what());
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count();
        if (sample == 0) output_bytes = size;
        test::require(output_bytes == size, "benchmark output changed between calls");
        if (sample >= 20) durations.push_back(elapsed);
    }
    std::sort(durations.begin(), durations.end());
    std::cout << "{\"profile\":\"" << profile << "\",\"phase\":\"" << phase << "\",\"rows\":" << rows
        << ",\"request_bytes\":" << request_bytes << ",\"output_bytes\":" << output_bytes
        << ",\"samples\":100,\"warmup\":20,\"p50_ns\":" << durations[49]
        << ",\"p95_ns\":" << durations[94] << ",\"p99_ns\":" << durations[98] << "}" << std::endl;
}
value limits() {
    return parse(R"({"max_input_bytes":67108864,"max_output_bytes":16777216,"max_documents":4096,
        "max_findings":65536,"max_instructions":1000000000,"max_milliseconds":30000})");
}
std::string batch(std::string_view profile, unsigned rows) {
    value::list documents;
    if (profile == "report-materialization-draft/1") {
        value::list source_rows;
        for (unsigned i = 0; i < rows; ++i)
            source_rows.emplace_back(value::object{{"amount", value(std::int64_t{i % 1000})},
                {"group", value("group_" + std::to_string(i % 32))}});
        documents.emplace_back(value::object{{"correlation", value("report")},
            {"fields", value(value::object{{"rows", value(std::move(source_rows))}})}, {"lines", value(value::object{})}});
    } else {
        for (unsigned i = 0; i < rows; ++i)
            documents.emplace_back(value::object{{"correlation", value("row_" + std::to_string(i))},
                {"fields", value(value::object{{"amount", value(std::int64_t{i % 1000})}})}, {"lines", value(value::object{})}});
    }
    return json_bytes(value(value::object{{"wire_version", value(std::int64_t{1})},
        {"documents", value(std::move(documents))}, {"limits", limits()}}));
}
}

void benchmark_native_profiles(std::string_view selected) {
    const std::vector<std::pair<std::string, value>> profiles = {
        {"formula-draft/1", parse(R"({"op":"add","type":"integer","args":[
            {"op":"field","type":"integer","field":"amount"},{"op":"literal","type":"integer","value":2}]})")},
        {"normalized-document-draft/1", parse(R"({"fields":[{"handle":"amount","required":true,"nullable":false,
            "validators":[{"rule":"integer"},{"rule":"min","value":"0"}]},
            {"handle":"total","formula":{"op":"multiply","type":"integer","args":[
            {"op":"field","type":"integer","field":"amount"},{"op":"literal","type":"integer","value":2}]}}],
            "invariants":[],"require_all":true})")},
        {"report-materialization-draft/1", parse(R"({"columns":[{"alias":"group","type":"string"},
            {"alias":"amount","type":"integer"}],"groups":[{"column":"group"}],
            "aggregates":[{"alias":"total","function":"sum","column":"amount"}],
            "formulas":[],"sorts":[{"output":"total","direction":"desc","nulls_last":true}]})")}
    };
    for (const auto& [profile, program] : profiles) {
        if (!selected.empty() && selected != profile) continue;
        const auto source = json_bytes(value(value::object{{"wire_version", value(std::int64_t{1})},
            {"profile", value(profile)}, {"corpus_digest", value(digest(profile))}, {"program", program}}));
        plan_owner cached; compile(source, cached);
        for (const unsigned rows : {1U, 32U, 256U, 4096U}) {
            const auto request = batch(profile, rows);
            measure(profile, "compile-and-execute", rows, source.size() + request.size(), [&]() {
                const std::string source_copy(source); plan_owner plan; compile(source_copy, plan);
                return execute(plan.handle, request);
            });
            measure(profile, "reused-plan", rows, request.size(), [&]() { return execute(cached.handle, request); });
        }
    }
    if (selected.empty() || selected == "normalized-preparation-draft/1") {
        std::ifstream stream(std::string(KUMWE_ENGINE_CORPUS_DIRECTORY) + "/document/preparation-v1.json", std::ios::binary);
        test::require(stream.good(), "preparation benchmark corpus");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto corpus = parse(bytes);
        const value* fixture = nullptr;
        for (const auto& item : corpus.at("fixtures").as<value::list>())
            if (item.at("id") == value("create_conditions_after_computation")) fixture = &item;
        test::require(fixture != nullptr && fixture->at("expected").at("returns_values") == value(true),
            "preparation benchmark requires a successful owner fixture");
        const std::string profile = "normalized-preparation-draft/1";
        const auto source = json_bytes(value(value::object{{"wire_version", value(std::int64_t{1})},
            {"profile", value(profile)}, {"corpus_digest", value(digest(profile))}, {"program", fixture->at("program")}}));
        plan_owner cached; compile(source, cached);
        for (const unsigned rows : {1U, 32U, 256U, 4096U}) {
            value::list documents;
            for (unsigned i = 0; i < rows; ++i)
                documents.emplace_back(value::object{{"correlation", value("row_" + std::to_string(i))},
                    {"fields", fixture->at("input")}, {"lines", fixture->at("lines")}});
            const auto request = json_bytes(value(value::object{{"wire_version", value(std::int64_t{1})},
                {"documents", value(std::move(documents))}, {"limits", limits()}}));
            measure(profile, "compile-and-execute", rows, source.size() + request.size(), [&]() {
                const std::string copied(source); plan_owner plan; compile(copied, plan);
                return execute(plan.handle, request);
            });
            measure(profile, "reused-plan", rows, request.size(), [&]() { return execute(cached.handle, request); });
        }
    }
    if (!selected.empty() && selected != "kumwe-canonical-json/generic-v1") return;
    const auto corpus = digest("kumwe-canonical-json/generic-v1");
    for (const unsigned rows : {1U, 32U, 256U, 4096U}) {
        value::list entries;
        for (unsigned i = 0; i < rows; ++i)
            entries.emplace_back(value::object{{"key", value(value::object{{"type", value("int")}, {"decimal", value(std::to_string(i))}})},
                {"value", value(value::object{{"type", value("int")}, {"decimal", value(std::to_string(i * 17))}})}});
        for (const auto* operation : {"encode", "digest"}) {
            const auto source = json_bytes(value(value::object{{"wire_version", value(std::int64_t{1})},
                {"profile", value("kumwe-canonical-json/generic-v1")}, {"corpus_digest", value(corpus)},
                {"operation", value(operation)}, {"input", value(value::object{{"type", value("array")}, {"entries", value(entries)}})}}));
            measure("kumwe-canonical-json/generic-v1", operation, rows, source.size(), [&]() {
                const std::string copy(source); const auto input = test::view(copy); test::response output;
                test::require(kumwe_engine_v1_canonical(&input, &output.buffer) == 0, "canonical benchmark");
                const auto result = output.bytes();
                test::require(result.find("\"finding\"") == std::string::npos, "canonical benchmark semantic refusal");
                return result.size();
            });
        }
    }
}
