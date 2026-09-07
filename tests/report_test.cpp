#include "support.hpp"
#include "reporting/report.hpp"
#include "vm/error.hpp"
#include "batch.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2, "report corpus path required");
        std::ifstream stream(argv[1],std::ios::binary);
        test::require(stream.good(), "report corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& item : corpus.at("fixtures").as<value::list>()) {
            const auto& id = item.at("id").as<std::string>();
            value::object computation;
            for (const auto key : {"columns","groups","aggregates","formulas","sorts"}) computation.emplace(key,item.at("plan").at(key));
            const auto plan = reporting::report_plan::compile(value(std::move(computation)));
            bool refused = false;
            value actual;
            std::uint64_t budget = 1000000000;
            try { actual = plan.materialize(item.at("authorized_rows"),budget); }
            catch (const vm::error& failure) { test::require(!failure.parse_phase,id + ": runtime refusal"); refused = true; }
            const auto* expected = item.at("expected").find("rows");
            if (expected) {
                test::require(!refused,id + ": unexpectedly refused");
                if (actual != *expected) {
                    std::cerr << id << " expected " << json::encode(*expected) << " actual " << json::encode(actual) << '\n';
                    return 1;
                }
            } else test::require(refused,id + ": expected refusal");
            ++count;
        }
        test::require(count == 116,"all frozen report materialization vectors replayed");
        auto rejects = [](std::string_view document) {
            bool rejected = false;
            try { (void)reporting::report_plan::compile(json::parse(document)); }
            catch (const vm::error& failure) { rejected = failure.parse_phase; }
            test::require(rejected,"invalid report plan refused at compilation");
        };
        rejects(R"({"columns":[{"alias":"n","type":"decimal"}],"query":"SELECT secret"})");
        rejects(R"({"columns":[{"alias":"n","type":"string"}],"aggregates":[{"alias":"s","function":"sum","column":"n"}]})");
        rejects(R"({"columns":[{"alias":"n","type":"integer"}],"groups":[{"column":"absent"}]})");
        rejects(R"({"columns":[{"alias":"n","type":"integer"}],"formulas":[{"alias":"x","type":"integer","expression":{"op":"field","type":"integer","field":"absent"}}]})");
        rejects(R"({"columns":[{"alias":"n","type":"integer"}],"formulas":[{"alias":"x","type":"integer","expression":{"op":"line_aggregate","type":"integer","lines":"hidden","aggregate":"count"}}]})");
        const auto plan = reporting::report_plan::compile(json::parse(R"({"columns":[{"alias":"n","type":"integer"}],"formulas":[{"alias":"double_n","type":"integer","expression":{"op":"multiply","type":"integer","args":[{"op":"field","type":"integer","field":"n"},{"op":"literal","type":"integer","value":2}]}},{"alias":"next","type":"integer","expression":{"op":"add","type":"integer","args":[{"op":"field","type":"integer","field":"double_n"},{"op":"literal","type":"integer","value":1}]}}],"sorts":[{"output":"next","direction":"desc","nulls_last":true}]})"));
        const auto input = json::parse(R"([{"n":2},{"n":3}])");
        std::uint64_t budget = 100000;
        test::require(plan.materialize(input,budget) == json::parse(R"([{"n":3,"double_n":6,"next":7},{"n":2,"double_n":4,"next":5}])"),"formula declaration dependencies and sorting");
        budget = 0;
        bool exhausted = false;
        try { (void)plan.materialize(input,budget); }
        catch (const refusal& failure) { exhausted = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(exhausted,"report execution budget enforced");
        budget = 100000;
        test::require(plan.materialize(input,budget).as<value::list>().size() == 2,"report plan reuse after refusal");
        budget = 100000;
        exhausted = false;
        try { (void)plan.materialize(input,budget,1); }
        catch (const refusal& failure) { exhausted = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(exhausted,"row admission bound enforced");
        budget = 100000;
        exhausted = false;
        try { (void)plan.materialize(input,budget,100000,2); }
        catch (const refusal& failure) { exhausted = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(exhausted,"report output bound enforced");
        bool structured = false;
        budget = 100000;
        try { (void)plan.materialize(json::parse(R"([{"n":{"secret":"value"}}])"),budget); }
        catch (const vm::error&) { structured = true; }
        test::require(structured,"structured report cells refused");
        std::cout << count << " frozen report vectors and compilation/formula/budget/reuse guards passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n'; return 1;
    }
}
