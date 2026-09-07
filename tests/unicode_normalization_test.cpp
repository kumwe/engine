#include "support.hpp"
#include "document/unicode_normalization.hpp"
#include "canonical/sha256.hpp"
#include "value/json.hpp"
#include "vm/error.hpp"
#include "batch.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

namespace {
std::string utf8(std::uint32_t cp) {
    std::string output;
    if (cp < 0x80) output.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        output.push_back(static_cast<char>(0xc0U | (cp >> 6)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    } else if (cp < 0x10000) {
        output.push_back(static_cast<char>(0xe0U | (cp >> 12)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 6) & 63U)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    } else {
        output.push_back(static_cast<char>(0xf0U | (cp >> 18)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 12) & 63U)));
        output.push_back(static_cast<char>(0x80U | ((cp >> 6) & 63U)));
        output.push_back(static_cast<char>(0x80U | (cp & 63U)));
    }
    return output;
}
std::string sequence(const std::string& source) {
    std::istringstream tokens(source);
    std::string output, item;
    while (tokens >> item) output += utf8(static_cast<std::uint32_t>(std::stoul(item, nullptr, 16)));
    return output;
}
std::string normalize(const std::string& text, std::string_view operation = "unicode_nfc", std::size_t limit = 1048576) {
    std::uint64_t budget = 10000000;
    return kumwe::engine::document::normalize_unicode(text, operation, budget, limit);
}
void feed(kumwe::engine::canonical::sha256& hash, const std::string& bytes) {
    const auto size = static_cast<std::uint32_t>(bytes.size());
    std::string prefix;
    for (unsigned i = 0; i < 4; ++i) prefix.push_back(static_cast<char>((size >> (24 - i * 8)) & 255U));
    hash.update(prefix); hash.update(bytes);
}
}
int main(int argc, char** argv) {
    using namespace kumwe::engine;
    try {
        test::require(argc == 2, "Unicode test source root argument");
        const std::string root = argv[1];
        std::ifstream conformance(root + "/third_party/unicode/15.1.0/NormalizationTest.txt");
        test::require(conformance.good(), "pinned NFC conformance file");
        std::string line;
        std::size_t vectors = 0;
        while (std::getline(conformance, line)) {
            line = line.substr(0, line.find('#'));
            if (line.empty() || line.front() == '@') continue;
            std::istringstream columns(line);
            std::array<std::string, 5> values;
            bool complete = true;
            for (auto& value : values) {
                std::string field;
                if (!std::getline(columns, field, ';')) { complete = false; break; }
                value = sequence(field);
            }
            if (!complete) continue;
            for (std::size_t i = 0; i < 5; ++i) {
                test::require(normalize(values[i]) == values[i < 3 ? 1U : 3U], "UAX15 NFC conformance vector");
            }
            ++vectors;
        }
        test::require(vectors == 19074, "all Unicode15.1 normalization vectors replayed");
        std::ifstream oracle_file(root + "/corpus/document/unicode-normalization-oracle-v1.json");
        test::require(oracle_file.good(), "frozen independent PHP Unicode oracle");
        const std::string oracle_bytes{std::istreambuf_iterator<char>(oracle_file), std::istreambuf_iterator<char>()};
        const auto oracle = json::parse(oracle_bytes);
        std::vector<std::uint32_t> contexts;
        for (const auto& cp : oracle.at("context_points").as<json::value::list>())
            contexts.push_back(static_cast<std::uint32_t>(cp.as<std::int64_t>()));
        const std::array<std::string_view, 3> operations{"lowercase", "uppercase", "unicode_nfc"};
        std::array<canonical::sha256, 3> hashes;
        canonical::sha256 context_hash;
        std::size_t scalar_count = 0, context_index = 0;
        for (std::uint32_t cp = 0; cp <= 0x10ffff; ++cp) {
            if (cp >= 0xd800 && cp <= 0xdfff) continue;
            const auto text = utf8(cp);
            for (std::size_t operation = 0; operation < operations.size(); ++operation)
                feed(hashes[operation], normalize(text, operations[operation]));
            if (context_index < contexts.size() && cp == contexts[context_index]) {
                for (const auto& probe : {text + "Σ", "A" + text + "Σ", "AΣ" + text, "AΣ" + text + "A"})
                    feed(context_hash, normalize(probe, "lowercase"));
                ++context_index;
            }
            ++scalar_count;
        }
        test::require(scalar_count == static_cast<std::size_t>(oracle.at("scalar_count").as<std::int64_t>()), "all Unicode scalars compared");
        for (std::size_t operation = 0; operation < operations.size(); ++operation) {
            const auto digest = hashes[operation].finish();
            if (digest != oracle.at("sha256").at(operations[operation]).as<std::string>()) {
                std::cerr << operations[operation] << " scalar digest " << digest << '\n';
                test::require(false, "independent PHP scalar differential");
            }
        }
        test::require(context_index == contexts.size(), "all casing property contexts checked");
        test::require(context_hash.finish() == oracle.at("context_sha256").as<std::string>(), "independent PHP Final_Sigma contexts");
        test::require(normalize("ß", "uppercase", 2) == "SS", "exact expansion output limit");
        test::require(normalize("A\xcc\x8a", "unicode_nfc", 2) == "Å", "NFC limit applies after composition");
        for (const auto operation : operations) {
            for (const auto& malformed : {std::string("\xc0\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xe2\x82")}) {
                bool refused = false;
                try { (void)normalize(malformed, operation); } catch (const vm::error&) { refused = true; }
                test::require(refused, "malformed UTF8 has typed normalization failure");
            }
            std::uint64_t budget = 0;
            bool refused = false;
            try { (void)document::normalize_unicode("x", operation, budget, 10); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "zero instruction budget");
        }
        for (const auto& item : std::array<std::pair<std::string, std::string_view>, 3>{{{"ß", "uppercase"}, {"İ", "lowercase"}, {"Å", "unicode_nfc"}}}) {
            bool refused = false;
            try { (void)normalize(item.first, item.second, 1); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "expanded UTF8 respects byte output limit");
        }
        std::uint64_t budget = 1000000;
        bool refused = false;
        try { (void)document::normalize_unicode(std::string(100000, 'A'), "unicode_nfc", budget, 1); }
        catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
        test::require(refused && budget > 999900, "NFC refuses impossible output before retaining a large intermediate");
        std::cout << vectors << " UAX15 vectors (95370 NFC comparisons), " << scalar_count
                  << " scalars across 3 PHP operations, " << context_index * 4 << " Final_Sigma contexts and limits passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
