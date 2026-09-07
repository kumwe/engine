#include "support.hpp"
#include "value/json.hpp"
#include "build_identity.hpp"
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

namespace {
void u32(std::string& output, std::size_t number) {
    for (unsigned i = 0; i < 4; ++i) output.push_back(static_cast<char>(number >> (8U * i)));
}
void framed(std::string& output, std::string_view input) { u32(output, input.size()); output.append(input); }
std::string binary_request(const kumwe::engine::json::value& request) {
    using value = kumwe::engine::json::value;
    auto metadata = request.as<value::object>(); metadata.erase("documents");
    std::string output = "KEB1"; framed(output, kumwe::engine::json::encode(value(metadata)));
    const auto& documents = request.at("documents").as<value::list>(); u32(output, documents.size());
    for (const auto& document : documents) {
        framed(output, document.at("correlation").as<std::string>());
        framed(output, document.at("input").as<std::string>());
    }
    return output;
}
std::string binary_response(const kumwe::engine::json::value& response) {
    using value = kumwe::engine::json::value;
    const auto& results = response.at("results").as<value::list>();
    std::string output = "KER2"; u32(output, results.size());
    for (const auto& result : results) {
        framed(output, result.at("correlation").as<std::string>());
        framed(output, kumwe::engine::json::encode(result.at("findings")));
        framed(output, result.at("result_json").as<std::string>());
    }
    return output;
}
struct plan_owner final {
    kumwe_engine_v1_plan* handle = nullptr;
    ~plan_owner() { kumwe_engine_v1_plan_release(&handle); }
};
struct cancellation_owner final {
    kumwe_engine_v1_cancellation* handle = nullptr;
    ~cancellation_owner() { kumwe_engine_v1_cancellation_release(&handle); }
};
}
int main() {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        auto compile = json::parse(R"({"wire_version":1,"profile":"formula-draft/1","corpus_digest":"x",
            "program":{"op":"add","type":"integer","args":[
            {"op":"field","type":"integer","field":"amount"},
            {"op":"literal","type":"integer","value":2}]}})");
        std::get<value::object>(compile.data)["corpus_digest"] = value(KUMWE_ENGINE_FORMULA_CORPUS);
        auto source = json::encode(compile);
        auto input = test::view(source);
        plan_owner plan;
        test::require(kumwe_engine_v1_compile(&input, &plan.handle) == 0, "compile immutable plan");
        const auto* same = plan.handle;
        test::require(kumwe_engine_v1_compile(&input, &plan.handle) == 1 && plan.handle == same, "nonempty plan slot preserved");
        test::require(kumwe_engine_v1_compile(&input, nullptr) == 1, "null plan slot refused");
        source.assign(source.size(), 'x');
        test::response description;
        test::require(kumwe_engine_v1_plan_describe(plan.handle, &description.buffer) == 0, "describe owned plan");
        test::require(json::parse(description.bytes()) == compile, "compile input not borrowed after return");
        auto opaque = compile;
        std::get<value::object>(opaque.data)["program"] = value(json::encode(compile.at("program")));
        auto opaque_bytes = json::encode(opaque); auto opaque_view = test::view(opaque_bytes); plan_owner opaque_plan;
        test::require(kumwe_engine_v1_compile(&opaque_view, &opaque_plan.handle) == 0, "opaque source compiled natively");
        std::get<value::object>(opaque.data)["program"] = value(R"({"op":"literal","type":"integer","value":1e-400})");
        opaque_bytes = json::encode(opaque); opaque_view = test::view(opaque_bytes); plan_owner underflow;
        test::require(kumwe_engine_v1_compile(&opaque_view, &underflow.handle) == 5 && underflow.handle == nullptr,
            "opaque binary64 underflow is not converted into an accepted integer zero");
        auto execution = json::parse(R"({"wire_version":1,"documents":[
            {"correlation":"row.1","fields":{"amount":5},"lines":{}},
            {"correlation":"row.2","fields":{"amount":9},"lines":{}}],
            "limits":{"max_input_bytes":100000,"max_output_bytes":10000,"max_documents":10,
            "max_findings":100,"max_instructions":100000,"max_milliseconds":30000}})");
        auto request = json::encode(execution);
        auto view = test::view(request);
        test::response result;
        test::require(kumwe_engine_v1_execute(plan.handle, &view, nullptr, &result.buffer) == 0, "coarse ordered batch");
        test::require(result.bytes() == R"({"results":[{"correlation":"row.1","findings":[],"result":{"findings":[],"value":7},"result_json":"{\"findings\":[],\"value\":7}"},{"correlation":"row.2","findings":[],"result":{"findings":[],"value":11},"result_json":"{\"findings\":[],\"value\":11}"}],"wire_version":1})",
            "streamed complete envelope preserves canonical payload bytes");
        for (const auto count : {std::size_t{1}, std::size_t{2}}) {
            auto exact = execution;
            std::get<value::list>(std::get<value::object>(exact.data).at("documents").data).resize(count);
            auto data = json::encode(exact); auto input = test::view(data); test::response complete;
            test::require(kumwe_engine_v1_execute(plan.handle, &input, nullptr, &complete.buffer) == 0,
                "measure complete output before applying exact byte budget");
            const auto expected_bytes = complete.bytes();
            auto& limits = std::get<value::object>(std::get<value::object>(exact.data).at("limits").data);
            for (unsigned short_by = 0; short_by < 2; ++short_by) {
                limits["max_output_bytes"] = value(static_cast<std::int64_t>(expected_bytes.size() - short_by));
                data = json::encode(exact); input = test::view(data); test::response bounded;
                const auto status = kumwe_engine_v1_execute(plan.handle, &input, nullptr, &bounded.buffer);
                test::require(short_by == 0 ? status == 0 && bounded.bytes() == expected_bytes
                    : status == 6 && bounded.buffer == nullptr, "nonempty batch exact output budget remains atomic");
            }
        }
        const auto expected = json::parse(R"({"wire_version":1,"results":[
            {"correlation":"row.1","result":{"value":7,"findings":[]}},
            {"correlation":"row.2","result":{"value":11,"findings":[]}}]})");
        auto opaque_execution = execution;
        auto& opaque_rows = std::get<value::list>(std::get<value::object>(opaque_execution.data).at("documents").data);
        for (auto& row : opaque_rows) row = value(value::object{{"correlation", row.at("correlation")},
            {"input", value(json::encode(value(value::object{{"fields", row.at("fields")}, {"lines", row.at("lines")}})))}});
        const auto opaque_request = json::encode(opaque_execution); auto opaque_input = test::view(opaque_request); test::response opaque_result;
        test::require(kumwe_engine_v1_execute(opaque_plan.handle, &opaque_input, nullptr, &opaque_result.buffer) == 0
            && opaque_result.bytes() == result.bytes(), "opaque whole-batch payload preserves exact output bytes");
        const auto framed_request = binary_request(opaque_execution);
        const auto framed_input = test::view(framed_request); test::response framed_result;
        const auto expected_framed = binary_response(json::parse(result.bytes()));
        test::require(kumwe_engine_v1_execute(opaque_plan.handle, &framed_input, nullptr, &framed_result.buffer) == 0
            && framed_result.bytes() == expected_framed, "framed batch returns identical correlation, findings and canonical bytes");
        for (unsigned short_by = 0; short_by < 2; ++short_by) {
            auto bounded = opaque_execution;
            std::get<value::object>(std::get<value::object>(bounded.data).at("limits").data)["max_output_bytes"] =
                value(static_cast<std::int64_t>(result.bytes().size() - short_by));
            const auto bytes = binary_request(bounded); const auto input = test::view(bytes); test::response output;
            const auto status = kumwe_engine_v1_execute(opaque_plan.handle, &input, nullptr, &output.buffer);
            test::require(short_by == 0 ? status == 0 && output.bytes() == expected_framed : status == 6 && output.buffer == nullptr,
                "framed output charges exactly the same logical JSON budget as legacy transport");
        }
        for (std::size_t length = 0; length < framed_request.size(); ++length) {
            const auto truncated = framed_request.substr(0, length); const auto input = test::view(truncated); test::response output;
            test::require(kumwe_engine_v1_execute(opaque_plan.handle, &input, nullptr, &output.buffer) != 0
                && output.buffer == nullptr, "every truncated batch frame refuses atomically");
        }
        const auto trailing = framed_request + "x"; const auto trailing_input = test::view(trailing); test::response trailing_result;
        test::require(kumwe_engine_v1_execute(opaque_plan.handle, &trailing_input, nullptr, &trailing_result.buffer) == 1
            && trailing_result.buffer == nullptr, "framed batch trailing bytes refused");
        auto exact_input = opaque_execution;
        auto& input_limits = std::get<value::object>(std::get<value::object>(exact_input.data).at("limits").data);
        for (unsigned i = 0; i < 3; ++i)
            input_limits["max_input_bytes"] = value(static_cast<std::int64_t>(json::encode(exact_input).size()));
        const auto logical_input_bytes = json::encode(exact_input).size();
        for (unsigned short_by = 0; short_by < 2; ++short_by) {
            input_limits["max_input_bytes"] = value(static_cast<std::int64_t>(logical_input_bytes - short_by));
            const auto bytes = binary_request(exact_input); const auto input = test::view(bytes); test::response output;
            const auto status = kumwe_engine_v1_execute(opaque_plan.handle, &input, nullptr, &output.buffer);
            test::require(short_by == 0 ? status == 0 && output.bytes() == expected_framed : status == 6 && output.buffer == nullptr,
                "framed input charges equivalent JSON bytes rather than bypassing limits with smaller transport");
        }
        const auto received = json::parse(result.bytes());
        const auto& received_rows = received.at("results").as<value::list>();
        const auto& expected_rows = expected.at("results").as<value::list>();
        test::require(received_rows.size() == expected_rows.size(), "one result per input");
        for (std::size_t i = 0; i < received_rows.size(); ++i) {
            test::require(received_rows[i].at("correlation") == expected_rows[i].at("correlation")
                && received_rows[i].at("result") == expected_rows[i].at("result"), "typed exact batch result");
            test::require(json::parse(received_rows[i].at("result_json").as<std::string>()) == expected_rows[i].at("result"),
                "opaque result bytes retain native value kinds");
        }
        cancellation_owner cancellation;
        test::require(kumwe_engine_v1_cancellation_create(&cancellation.handle) == 0, "cancellation owner");
        kumwe_engine_v1_cancellation_request(cancellation.handle);
        test::response cancelled;
        test::require(kumwe_engine_v1_execute(plan.handle, &view, cancellation.handle, &cancelled.buffer) == 7
            && cancelled.buffer == nullptr, "cancelled batch emits no partial result");
        test::require(kumwe_engine_v1_execute(plan.handle, &view, nullptr, &cancelled.buffer) == 0, "cancelled plan remains reusable");
        std::atomic<bool> success{true};
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < 4; ++i) threads.emplace_back([&]() {
            for (unsigned repeat = 0; repeat < 25; ++repeat) {
                test::response output;
                if (kumwe_engine_v1_execute(plan.handle, &view, nullptr, &output.buffer) != 0
                    || output.bytes() != result.bytes()) success.store(false);
            }
        });
        for (auto& thread : threads) thread.join();
        test::require(success.load(), "same immutable plan executes concurrently");
        for (const auto* key : {"max_input_bytes", "max_output_bytes", "max_documents", "max_instructions"}) {
            auto bounded = execution;
            std::get<value::object>(std::get<value::object>(bounded.data).at("limits").data)[key] = value(std::int64_t{1});
            auto data = json::encode(bounded); auto limit_view = test::view(data); test::response refused;
            test::require(kumwe_engine_v1_execute(plan.handle, &limit_view, nullptr, &refused.buffer) == 6
                && refused.buffer == nullptr, "caller budgets atomically enforced");
        }
        {
            constexpr std::string_view empty_bytes = R"({"results":[],"wire_version":1})";
            auto empty = execution;
            std::get<value::object>(empty.data)["documents"] = value(value::list{});
            auto& limits = std::get<value::object>(std::get<value::object>(empty.data).at("limits").data);
            for (unsigned short_by = 0; short_by < 2; ++short_by) {
                limits["max_output_bytes"] = value(static_cast<std::int64_t>(empty_bytes.size() - short_by));
                const auto data = json::encode(empty); const auto input = test::view(data); test::response output;
                const auto status = kumwe_engine_v1_execute(plan.handle, &input, nullptr, &output.buffer);
                test::require(short_by == 0 ? status == 0 && output.bytes() == empty_bytes
                    : status == 6 && output.buffer == nullptr, "empty streamed envelope exact output budget remains atomic");
            }
        }
        {
            auto bounded = execution;
            auto& limits = std::get<value::object>(std::get<value::object>(bounded.data).at("limits").data);
            limits["max_input_bytes"] = value(std::int64_t{67108864});
            limits["max_milliseconds"] = value(std::int64_t{1});
            auto data = json::encode(bounded);
            data.append(33554432, ' ');
            auto limited = test::view(data); test::response refused;
            test::require(kumwe_engine_v1_execute(plan.handle, &limited, nullptr, &refused.buffer) == 6
                && refused.buffer == nullptr, "execution deadline includes native envelope decoding");
        }
        for (const auto* key : {"max_input_bytes", "max_output_bytes", "max_instructions"}) {
            auto bounded = compile;
            std::get<value::object>(bounded.data)["limits"] = execution.at("limits");
            std::get<value::object>(std::get<value::object>(bounded.data).at("limits").data)[key] = value(std::int64_t{1});
            const auto data = json::encode(bounded); auto limited = test::view(data); plan_owner refused;
            test::require(kumwe_engine_v1_compile(&limited, &refused.handle) == 6 && refused.handle == nullptr,
                "compile input/work/output budgets enforced before plan admission");
        }
        for (unsigned variant = 0; variant < 4; ++variant) {
            auto malformed = compile;
            auto& fields = std::get<value::object>(malformed.data);
            if (variant == 0) fields["wire_version"] = value(std::int64_t{2});
            if (variant == 1) fields["profile"] = value("unavailable/1");
            if (variant == 2) fields["corpus_digest"] = value("wrong");
            if (variant == 3) fields["program"] = value(value::object{});
            auto data = json::encode(malformed); auto invalid = test::view(data); plan_owner refused;
            test::require(kumwe_engine_v1_compile(&invalid, &refused.handle) == variant + 2U
                && refused.handle == nullptr, "identity/program failures remain distinct");
        }
        kumwe_engine_v1_plan_release(&plan.handle);
        kumwe_engine_v1_plan_release(&plan.handle);
        kumwe_engine_v1_cancellation_release(&cancellation.handle);
        kumwe_engine_v1_cancellation_release(&cancellation.handle);
        test::require(plan.handle == nullptr && cancellation.handle == nullptr, "owners clear after repeated cleanup");
        std::cout << "Immutable plan ABI, exact batch, limits, cancellation and concurrent ownership tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
