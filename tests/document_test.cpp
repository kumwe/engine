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
        test::require(argc == 2 || argc == 3, "document corpus path and optional expected count");
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
                for (const auto key : {"handle", "required", "nullable", "formula", "validators", "type", "precision", "scale", "length", "normalizers"})
                    if (const auto* item = field.find(key)) projected.emplace(key, *item);
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
            auto owned_fields = item.at("normalized_values");
            std::uint64_t owned_work = 1000000;
            test::require(compiled.execute_owned(std::move(owned_fields), lines, owned_work, 1000, 1000000) == actual
                && owned_work == work, "owned and borrowed document paths preserve exact result and work charges");
            const auto output_bytes = json::encode(actual).size();
            for (const auto limit : {std::size_t{0}, std::size_t{1}, output_bytes - 1, output_bytes, output_bytes + 1}) {
                std::uint64_t borrowed_budget = 1000000, owned_budget = 1000000;
                std::uint32_t borrowed_status = 0, owned_status = 0;
                value borrowed_result, owned_result;
                try { borrowed_result = compiled.execute(item.at("normalized_values"), lines, borrowed_budget, 1000, limit); }
                catch (const refusal& e) { borrowed_status = e.code; }
                try { owned_result = compiled.execute_owned(value(item.at("normalized_values")), lines, owned_budget, 1000, limit); }
                catch (const refusal& e) { owned_status = e.code; }
                test::require(borrowed_status == owned_status && borrowed_budget == owned_budget,
                    "frozen document exact-byte crossings preserve owned/borrowed status and work");
                if (borrowed_status == 0) test::require(borrowed_result == actual && owned_result == actual
                    && output_bytes <= limit, "successful retained-byte accounting matches serialized result");
            }
            ++count;
        }
        test::require(count == (argc == 3 ? static_cast<std::size_t>(std::stoul(argv[2])) : 10), "all frozen document computation/validation vectors replayed");
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
        auto extra_fields = fields;
        std::get<value::object>(extra_fields.data)["unreferenced"] = value("retained");
        budget = 100000;
        test::require(plan.execute(extra_fields, lines, budget, 100, 10000).at("values").at("unreferenced") == value("retained"),
            "unreferenced input remains in canonical output");
        std::get<value::object>(extra_fields.data)["unreferenced"] = json::parse(R"({"type":"normalized-value","version":2,"kind":"datetime","value":"2026-09-07T00:00:00.000000+00:00"})");
        budget = 100000;
        bool invalid_unreferenced = false;
        try { (void)plan.execute(extra_fields, lines, budget, 100, 10000); }
        catch (const refusal& failure) { invalid_unreferenced = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(invalid_unreferenced, "unreferenced normalized input still receives complete admission");
        budget = 100000;
        invalid_unreferenced = false;
        try { (void)plan.execute_owned(std::move(extra_fields), lines, budget, 100, 10000); }
        catch (const refusal& failure) { invalid_unreferenced = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(invalid_unreferenced, "owned unreferenced normalized input still receives complete admission");
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
            const auto remaining = budget;
            budget = which == 0 ? 0 : 100000;
            refused = false;
            try { (void)plan.execute_owned(value(value::object{}), lines, budget, which == 1 ? 1 : 100, which == 2 ? 1 : 10000); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused && budget == remaining, "owned and borrowed budget crossings preserve refusal and work charges");
        }
        const auto patterned = document::plan::compile(json::parse(R"({"fields":[{"handle":"x","validators":[{"rule":"pattern","value":"x"}]}],"invariants":[]})"));
        budget = 100000;
        test::require(patterned.execute(json::parse(R"({"x":"x"})"), lines, budget, 100, 10000).at("findings") == value(value::list{}), "bounded pattern executes");
        value::list copies;
        for (unsigned i = 0; i < 512; ++i) copies.emplace_back(value::object{{"handle", value("copy" + std::to_string(i))}, {"length", value(std::int64_t{16384})},
            {"formula", json::parse(R"({"op":"field","type":"string","field":"input"})")}});
        const auto expanding = document::plan::compile(value(value::object{{"fields", value(copies)}, {"invariants", value(value::list{})}}));
        budget = 1000000;
        bool refused = false;
        try { (void)expanding.execute(value(value::object{{"input", value(std::string(16384, 'x'))}}), lines, budget, 100, 32768); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(refused && 1000000 - budget < 16384 + 10, "aggregate retained output refuses before copying 512 computed fields");
        value::list nodes(4095, value());
        test::require(document::normalized_value_valid(value(nodes)), "4096 semantic normalized nodes admitted");
        nodes.emplace_back();
        test::require(!document::normalized_value_valid(value(nodes)), "4097 semantic normalized nodes refused");
        value nested;
        for (unsigned i = 0; i < 8; ++i) nested = value(value::list{std::move(nested)});
        test::require(document::normalized_value_valid(nested), "eight normalized array levels admitted");
        nested = value(value::list{std::move(nested)});
        test::require(!document::normalized_value_valid(nested), "ninth normalized array level refused");
        for (const auto malformed : {
            R"({"type":"normalized-value","version":2,"kind":"datetime","value":"2026-09-07T00:00:00.000000+00:00"})",
            R"({"type":"normalized-value","version":1,"kind":"array","value":{"entries":[{"key":"0","value":null}]}})",
            R"({"type":"normalized-value","version":1,"kind":"array","value":{"entries":[{"key":"a","value":1},{"key":"a","value":2}]}})",
            R"({"type":"exact-decimal","value":"01.00"})"})
            test::require(!document::normalized_value_valid(json::parse(malformed)), "malformed normalized tag refused");
        std::cout << count << " frozen document vectors plus dependency, findings, ownership and budget tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
