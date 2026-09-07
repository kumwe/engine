#include "support.hpp"
#include "vm/formula.hpp"
#include "vm/error.hpp"
#include "batch.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2, "formula corpus path required");
        std::ifstream stream(argv[1], std::ios::binary);
        test::require(stream.good(), "formula corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& item : corpus.at("vectors").as<value::list>()) {
            const auto& id = item.at("id").as<std::string>();
            value actual;
            bool parsing = true;
            try {
                const auto plan = vm::formula::compile(item.at("expression"));
                test::require(plan.document() == item.at("canonical"), id + ": canonical AST");
                value::list dependencies;
                for (const auto& field : plan.dependencies()) dependencies.emplace_back(field);
                test::require(value(dependencies) == item.at("dependencies"), id + ": dependencies");
                value::object lines;
                for (const auto& [collection, fields] : plan.line_dependencies()) {
                    value::list entries;
                    for (const auto& field : fields) entries.emplace_back(field);
                    lines.emplace(collection, value(entries));
                }
                test::require(value(lines) == item.at("line_dependencies"), id + ": line dependencies");
                parsing = false;
                std::uint64_t budget = 1000000000;
                actual = value(value::object{{"value", plan.evaluate(item.at("fields"), item.at("lines"), budget)}});
            } catch (const vm::error& failure) {
                test::require(failure.parse_phase == parsing, id + ": refusal phase");
                actual = value(value::object{
                    {"refusal", value(parsing ? "invalid_ast" : "evaluation_refused")},
                    {"phase", value(parsing ? "parse" : "evaluate")},
                    {"message", value(failure.what())}});
            }
            if (actual != item.at("expected")) {
                std::cerr << id << " expected " << json::encode(item.at("expected"))
                          << " actual " << json::encode(actual) << '\n';
                return 1;
            }
            ++count;
        }
        test::require(count == 101, "all frozen formula vectors replayed");
        const auto plan = vm::formula::compile(json::parse("{\"op\":\"literal\",\"type\":\"integer\",\"value\":7}"));
        const value empty(value::object{});
        std::uint64_t budget = 0;
        bool exhausted = false;
        try { (void)plan.evaluate(empty, empty, budget); }
        catch (const refusal& failure) { exhausted = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(exhausted, "execution work budget enforced");
        budget = 1;
        test::require(plan.evaluate(empty, empty, budget) == value(std::int64_t{7}), "immutable plan reusable after refusal");
        std::cout << count << " formula corpus vectors and execution budget/reuse checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
