#include "support.hpp"
#include "document/preparation.hpp"
#include "plan/plan.hpp"
#include "batch.hpp"
#include "build_identity.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2, "preparation corpus path");
        std::ifstream stream(argv[1], std::ios::binary);
        test::require(stream.good(), "preparation corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& fixture : corpus.at("fixtures").as<value::list>()) {
            try {
            const auto compiled = preparation::plan::compile(fixture.at("program"));
            const auto restored = [&] {
                const auto temporary = preparation::plan::compile(compiled.document());
                auto copied = temporary;
                return preparation::plan(std::move(copied));
            }();
            std::uint64_t budget = 1000000;
            const auto actual = restored.execute(fixture.at("input"), fixture.at("lines"), budget, 1000, 1000000);
            std::uint64_t repeated_budget = 1000000;
            const auto repeated = restored.execute(fixture.at("input"), fixture.at("lines"), repeated_budget, 1000, 1000000);
            test::require(repeated == actual && repeated_budget == budget,
                "copied and moved plans retain no original-plan or previous-execution references");
            const auto& expected = fixture.at("expected");
            if (actual.at("findings") != expected.at("findings")
                || (expected.at("returns_values") == value(true) && actual.at("values") != expected.at("values"))) {
                std::cerr << fixture.at("id").as<std::string>() << " expected " << json::encode(expected)
                          << " actual " << json::encode(actual) << '\n';
                return 1;
            }
            ++count;
            } catch (const refusal& failure) {
                std::cerr << fixture.at("id").as<std::string>() << " refused " << failure.code << '\n';
                return 1;
            }
        }
        test::require(count == 43, "all unchanged PHP create/update vectors replayed");
        const auto& sample = corpus.at("fixtures").as<value::list>().front();
        const auto compiled = preparation::plan::compile(sample.at("program"));
        for (unsigned which = 0; which < 3; ++which) {
            std::uint64_t budget = which == 0 ? 0 : 1000000;
            auto input = sample.at("input");
            if (which == 1) {
                std::get<value::object>(input.data).at("input") = json::parse(R"([
                    {"handle":"missing","submitted":1,"normalized":{"value":null,"valid":true}}])");
            }
            bool refused = false;
            try { (void)compiled.execute(input, value(), budget, which == 1 ? 0 : 100, which == 2 ? 1 : 1000000); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "preparation shares instruction/finding/output limits");
        }
        auto duplicate = sample.at("input");
        std::get<value::object>(duplicate.data).at("input") = json::parse(R"([
            {"handle":"name","submitted":"x","normalized":{"value":"x","valid":true}},
            {"handle":"name","submitted":"y","normalized":{"value":"y","valid":true}}])");
        std::uint64_t budget = 100000;
        bool refused = false;
        try { (void)compiled.execute(duplicate, value(), budget, 100, 100000); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(refused, "ordered input cannot contain duplicate PHP map keys");

        auto unused_invalid = sample.at("input");
        std::get<value::object>(unused_invalid.data).at("input") = json::parse(R"([
            {"handle":"undeclared","submitted":null,"normalized":{"valid":true,
                "value":{"type":"exact-decimal","value":"invalid"}}}])");
        budget = 100000;
        refused = false;
        try { (void)compiled.execute(unused_invalid, value(), budget, 100, 100000); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(refused, "unused values still undergo admission before condition projection");

        auto instance_default = sample.at("program");
        auto& default_field = std::get<value::list>(std::get<value::object>(instance_default.data).at("fields").data).front();
        std::get<value::object>(default_field.data).at("default") = json::parse(R"({"valid":true,
            "value":{"type":"exact-decimal","value":"1.00","instance":1}})");
        refused = false;
        try { (void)preparation::plan::compile(instance_default); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_INVALID_PROGRAM; }
        test::require(refused, "reusable defaults cannot carry execution-local instance tokens");

        const value* same_instance = nullptr;
        for (const auto& fixture : corpus.at("fixtures").as<value::list>()) {
            if (fixture.at("id") == value("update_immutable_money_same_instance")) same_instance = &fixture;
        }
        test::require(same_instance != nullptr, "actual same-instance PHP fixture exists");
        const auto instance_plan = preparation::plan::compile(same_instance->at("program"));
        for (unsigned which = 0; which < 2; ++which) {
            auto instance_request = same_instance->at("input");
            auto& amount = std::get<value::object>(std::get<value::object>(instance_request.data).at("current").data).at("amount");
            if (which == 0) {
                auto& payload = std::get<value::object>(std::get<value::object>(amount.data).at("value").data);
                payload.at("amount") = value("99.00");
            } else std::get<value::object>(amount.data).erase("instance");
            budget = 100000;
            refused = false;
            try { (void)instance_plan.execute(instance_request, value(), budget, 100, 100000); }
            catch (const refusal& failure) {
                refused = failure.code == (which == 0 ? KUMWE_ENGINE_V1_INVALID_INPUT : KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY);
            }
            test::require(refused, "conflicting or missing object identity refuses without inventing an immutable finding");
        }

        auto wide = sample.at("program");
        value::list wide_fields, validation_fields;
        for (unsigned index = 0; index < 32; ++index) {
            auto declaration = wide.at("fields").as<value::list>().front().as<value::object>();
            const value name("field" + std::to_string(index));
            declaration.at("handle") = name;
            declaration.at("identity") = value(false);
            declaration.at("read_only") = value(false);
            declaration.at("default") = value(value::object{{"valid", value(true)},
                {"value", value(std::string(256, 'x'))}});
            wide_fields.emplace_back(std::move(declaration));
            auto validation = wide.at("validation").at("fields").as<value::list>().front().as<value::object>();
            validation.at("handle") = name;
            validation.insert_or_assign("type", value("core.text"));
            validation_fields.emplace_back(std::move(validation));
        }
        std::get<value::object>(wide.data).at("fields") = value(std::move(wide_fields));
        std::get<value::object>(std::get<value::object>(wide.data).at("validation").data).at("fields")
            = value(std::move(validation_fields));
        budget = 1000;
        refused = false;
        try { (void)preparation::plan::compile(wide).execute(sample.at("input"), value(), budget, 100, 512); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(refused && budget > 980, "default output refuses before retaining all 32 large values");

        const auto limits = json::parse(R"({"max_input_bytes":1000000,"max_output_bytes":1000000,
            "max_documents":100,"max_findings":1000,"max_instructions":1000000,"max_milliseconds":30000})");
        const value envelope(value::object{{"wire_version", value(std::int64_t{1})},
            {"profile", value("normalized-preparation-draft/1")},
            {"corpus_digest", value(KUMWE_ENGINE_PREPARATION_CORPUS)}, {"program", sample.at("program")}});
        const auto executable = execution::plan::compile(json::encode(envelope));
        const value request(value::object{{"wire_version", value(std::int64_t{1})}, {"limits", limits},
            {"documents", value(value::list{value(value::object{{"correlation", value("prepared")},
                {"fields", sample.at("input")}, {"lines", value()}})})}});
        const auto result = json::parse(executable.execute(json::encode(request), nullptr));
        test::require(result.at("results").as<value::list>().front().at("result").at("values")
            == sample.at("expected").at("values"), "coarse plan dispatch executes preparation profile");
        std::cout << count << " PHP preparation vectors, ordered findings, admission and budget checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
