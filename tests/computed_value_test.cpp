#include "support.hpp"
#include "batch.hpp"
#include "document/computed_value.hpp"
#include "vm/error.hpp"
#include "vm/formula.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2, "computed normalization oracle path required");
        std::ifstream stream(argv[1], std::ios::binary);
        test::require(stream.good(), "computed normalization oracle readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t replayed = 0;
        for (const auto& fixture : corpus.at("fixtures").as<value::list>()) {
            const auto& id = fixture.at("id").as<std::string>();
            const auto& field = fixture.at("definition").at("fields").as<value::list>().at(1);
            document::validate_computed_definition(field);
            const auto compiled = vm::formula::compile(field.at("formula"));
            std::uint64_t budget = 1000000;
            auto raw = compiled.evaluate(fixture.at("normalized_values"), value(value::object{}), budget, 1000000);
            value normalized;
            bool failed = false;
            try { normalized = document::normalize_computed_value(std::move(raw), field, budget, 1000000); }
            catch (const vm::error&) { failed = true; }
            bool expected_failure = false;
            for (const auto& finding : fixture.at("expected").at("findings").as<value::list>()) {
                if (finding.at("field") == field.at("handle") && finding.at("code") == value("formula_failed")) expected_failure = true;
            }
            test::require(failed == expected_failure, id + ": exact PHP normalization admission");
            if (!failed) {
                const auto& expected = fixture.at("expected").at("values").at(field.at("handle").as<std::string>());
                const auto& scalar = normalized.is<value::object>() ? normalized.at("value") : normalized;
                test::require(scalar == expected, id + ": exact PHP normalized bytes");
                const auto& runtime = fixture.at("oracle_runtime_types").at(field.at("handle").as<std::string>()).as<std::string>();
                if (runtime == "DateTimeImmutable") {
                    test::require(normalized.at("type") == value("normalized-value") && normalized.at("kind") == value("datetime"),
                        id + ": temporal provenance survives normalization");
                } else if (runtime.find("ExactDecimal") != std::string::npos) {
                    test::require(normalized.at("type") == value("exact-decimal"), id + ": exact-decimal provenance survives normalization");
                } else test::require(!normalized.is<value::object>(), id + ": scalar stays scalar");
            }
            ++replayed;
        }
        test::require(replayed == 86, "all 86 exact PHP normalizations replayed");
        const auto valid = json::parse(R"({"type":"core.computed","formula":{"op":"literal","type":"decimal","value":"2.5"},"precision":5,"scale":2})");
        for (unsigned variant = 0; variant < 5; ++variant) {
            auto malformed = valid;
            auto& fields = std::get<value::object>(malformed.data);
            if (variant == 0) fields.erase("scale");
            if (variant == 1) fields["scale"] = value(std::int64_t{31});
            if (variant == 2) fields["length"] = value(std::int64_t{0});
            if (variant == 3) fields["normalizers"] = value(value::list{value("trim"), value("trim")});
            if (variant == 4) fields["normalizers"] = value("trim");
            bool rejected = false;
            try { document::validate_computed_definition(malformed); }
            catch (const refusal& error) { rejected = error.code == KUMWE_ENGINE_V1_INVALID_PROGRAM; }
            test::require(rejected, "malformed computed codec metadata fails at compilation");
        }
        for (unsigned variant = 0; variant < 2; ++variant) {
            std::uint64_t budget = variant == 0 ? 1 : 100000;
            bool rejected = false;
            try { (void)document::normalize_computed_value(value("2.5"), valid, budget, variant == 0 ? 1000 : 1); }
            catch (const refusal& error) { rejected = error.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(rejected, "normalization work and output limits remain whole-call refusals");
        }
        std::cout << replayed << " PHP computed-normalization cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
