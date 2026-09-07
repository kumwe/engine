#include "support.hpp"
#include "document/document.hpp"
#include "batch.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2, "document corpus path");
        std::ifstream stream(argv[1], std::ios::binary);
        test::require(stream.good(), "document corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& item : corpus.at("fixtures").as<value::list>()) {
            value::list fields, invariants;
            const auto& definition = item.at("definition");
            for (const auto& field : definition.at("fields").as<value::list>()) {
                value::object projected;
                for (const auto key : {"handle", "required", "nullable", "formula", "validators"})
                    projected.emplace(key, field.at(key));
                fields.emplace_back(std::move(projected));
            }
            if (const auto* rules = definition.find("record_invariants")) {
                for (const auto& rule : rules->as<value::list>()) invariants.emplace_back(value::object{
                    {"handle", rule.at("handle")}, {"condition", rule.at("condition")}});
            }
            const auto compiled = document::plan::compile(value(value::object{
                {"fields", value(std::move(fields))}, {"invariants", value(std::move(invariants))}}));
            std::uint64_t work = 1000000;
            auto lines = item.at("owned_lines");
            // PHP's empty associative array is [] in its frozen transport. The native
            // typed-map envelope spells it {} while null continues to mean suspended.
            if (lines.is<value::list>() && lines.as<value::list>().empty()) lines = value(value::object{});
            const auto actual = compiled.execute(item.at("normalized_values"), lines, work, 1000, 1000000);
            if (actual != item.at("expected")) {
                std::cerr << item.at("id").as<std::string>() << " expected " << json::encode(item.at("expected"))
                          << " actual " << json::encode(actual) << '\n';
                return 1;
            }
            ++count;
        }
        test::require(count == 10, "all frozen document computation/validation vectors replayed");
        const auto source = json::parse(R"({"fields":[
            {"handle":"total","formula":{"op":"add","type":"integer","args":[
              {"op":"field","type":"integer","field":"subtotal"},
              {"op":"literal","type":"integer","value":3}]}},
            {"handle":"subtotal","formula":{"op":"multiply","type":"integer","args":[
              {"op":"field","type":"integer","field":"quantity"},
              {"op":"field","type":"integer","field":"price"}]}},
            {"handle":"label","required":true,"nullable":false,
             "validators":[{"rule":"max_length","value":2}]}
          ],"invariants":[{"handle":"positive","condition":{"op":"gt","type":"boolean","args":[
            {"op":"field","type":"integer","field":"total"},
            {"op":"literal","type":"integer","value":0}]}}]})");
        const auto plan = document::plan::compile(source);
        const auto round_trip = document::plan::compile(plan.document());
        const auto fields = json::parse(R"({"quantity":2,"price":5,"label":"é界"})");
        const value lines(value::object{});
        std::uint64_t budget = 100000;
        const auto output = round_trip.execute(fields, lines, budget, 100, 10000);
        test::require(output.at("values").at("total") == value(std::int64_t{13}), "dependency passes");
        test::require(output.at("findings") == value(value::list{}), "UTF8 character length");
        test::require(fields.find("total") == nullptr, "input remains immutable");
        budget = 100000;
        const auto missing = plan.execute(value(value::object{}), lines, budget, 100, 10000);
        const auto expected = json::parse(R"([
            {"field":"total","code":"formula_dependency"},
            {"field":"subtotal","code":"formula_dependency"},
            {"field":"label","code":"required"},
            {"field":"positive","code":"invariant_invalid"}])");
        test::require(missing.at("findings") == expected, "findings preserve source discovery order");
        for (unsigned which = 0; which < 3; ++which) {
            budget = which == 0 ? 0 : 100000;
            bool refused = false;
            try { (void)plan.execute(value(value::object{}), lines, budget, which == 1 ? 1 : 100, which == 2 ? 1 : 10000); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "instruction/finding/output budget enforced");
        }
        bool refused = false;
        try { (void)document::plan::compile(json::parse(R"({"fields":[{"handle":"x","validators":[{"rule":"pattern","value":"x"}]}],"invariants":[]})")); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY; }
        test::require(refused, "unimplemented semantic profiles refuse compilation");
        std::cout << count << " frozen document vectors plus dependency, findings, ownership and budget tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
