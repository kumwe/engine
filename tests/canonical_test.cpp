#include "canonical/canonical.hpp"
#include "canonical/sha256.hpp"
#include "support.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

using namespace kumwe::engine;
namespace {
canonical::value fixture(const json::value& input) {
    const auto& kind = input.at("type").as<std::string>();
    if (kind == "repeat-string") {
        const auto unit = canonical::from_tagged(json::value(json::value::object{{"type", "string"}, {"base64", input.at("base64")}}));
        const auto& text = std::get<std::string>(unit.data);
        const auto count = static_cast<std::size_t>(input.at("count").as<std::int64_t>());
        test::require(count <= 8388608 && text.size() <= 8, "fixture expansion bound");
        std::string result;
        result.reserve(count * text.size());
        for (std::size_t i = 0; i < count; ++i) result += text;
        return canonical::value(std::move(result));
    }
    if (kind == "nested-list") {
        auto leaf = fixture(input.at("leaf"));
        const auto depth = input.at("depth").as<std::int64_t>();
        test::require(depth >= 0 && depth <= 65, "nested fixture bound");
        for (std::int64_t i = 0; i < depth; ++i) {
            canonical::value::array result;
            result.emplace_back(std::int64_t{0}, std::move(leaf));
            leaf = canonical::value(std::move(result));
        }
        return leaf;
    }
    if (kind == "repeat-list") {
        const auto leaf = fixture(input.at("value"));
        const auto count = input.at("count").as<std::int64_t>();
        test::require(count >= 0 && count <= 100000, "wide fixture bound");
        canonical::value::array result;
        result.reserve(static_cast<std::size_t>(count));
        for (std::int64_t i = 0; i < count; ++i) result.emplace_back(i, leaf);
        return canonical::value(std::move(result));
    }
    return canonical::from_tagged(input);
}
}
int main(int argc, char** argv) {
    try {
        test::require(argc == 2, "canonical corpus path required");
        std::ifstream stream(argv[1], std::ios::binary);
        test::require(stream.good(), "canonical corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        canonical::sha256 corpus_hash;
        corpus_hash.update(bytes);
        test::require(corpus_hash.finish() == "84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250", "frozen upstream corpus digest");
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& item : corpus.at("cases").as<json::value::list>()) {
            const auto& id = item.at("id").as<std::string>();
            const auto source = fixture(item.at("input"));
            const auto* custom = item.find("limits");
            const auto bounds = custom == nullptr ? canonical::limits{} : canonical::limits_from_json(*custom);
            const auto* expected_finding = item.at("expected").find("finding");
            for (const bool digest : {false, true}) {
                try {
                    const auto result = digest ? canonical::digest(source, bounds) : canonical::encode(source, bounds);
                    test::require(expected_finding == nullptr, id + ": expected refusal");
                    const auto& expected = item.at("expected").at(digest ? "sha256" : "output").as<std::string>();
                    if (result != expected) {
                        std::cerr << id << (digest ? " digest" : " bytes") << " expected " << expected << " actual " << result << '\n';
                        return 1;
                    }
                } catch (const canonical::error& failure) {
                    test::require(expected_finding != nullptr, id + ": unexpected refusal " + failure.what());
                    test::require(expected_finding->as<std::string>() == failure.what(), id + ": refusal order " + failure.what());
                }
            }
            const auto& input_kind = item.at("input").at("type").as<std::string>();
            if (input_kind != "repeat-string" && input_kind != "repeat-list" && input_kind != "nested-list") {
                for (const bool hashing : {false, true}) {
                    json::value::object request{{"profile", "kumwe-canonical-json/generic-v1"},
                        {"operation", hashing ? "digest" : "encode"}, {"input", item.at("input")}};
                    if (custom != nullptr) request.emplace("limits", *custom);
                    try {
                        const auto actual = canonical::evaluate(json::value(request));
                        test::require(expected_finding == nullptr, id + ": bridge expected refusal");
                        const auto key = hashing ? "sha256" : "output";
                        test::require(actual.at(key) == item.at("expected").at(key), id + ": bridge bytes/digest");
                    } catch (const canonical::error& failure) {
                        test::require(expected_finding != nullptr, id + ": bridge unexpected refusal");
                        test::require(expected_finding->as<std::string>() == failure.what(), id + ": bridge refusal order " + failure.what());
                    }
                    request.emplace("wire_version", json::value(std::int64_t{1}));
                    request.emplace("corpus_digest", "84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250");
                    const auto wire = json::encode(json::value(request));
                    const auto view = test::view(wire);
                    test::response owner;
                    test::require(kumwe_engine_v1_canonical(&view, &owner.buffer) == KUMWE_ENGINE_V1_OK, id + ": C ABI owned result status");
                    const auto actual = json::parse(owner.bytes());
                    const auto key = expected_finding != nullptr ? "finding" : (hashing ? "sha256" : "output");
                    test::require(actual.at(key) == item.at("expected").at(key), id + ": C ABI semantic bytes/digest/finding");
                    kumwe_engine_v1_buffer_release(&owner.buffer);
                    test::require(owner.buffer == nullptr, id + ": C ABI releases owned result");
                }
            }
            ++count;
        }
        test::require(count == 79, "all 79 canonical fixtures replayed");
        // Frozen PHP 8.5.10 oracle with serialize_precision=-1: adjacent ULPs at
        // fixed/scientific notation and large-significand transitions, both signs.
        for (const auto& [bits, expected] : std::vector<std::pair<std::uint64_t, std::string>>{
            {0xbee4f8b588e368f2ULL, "-1.0000000000000003e-5"},
            {0x3ee4f8b588e368f0ULL, "9.999999999999999e-6"},
            {0xbee4f8b588e368f0ULL, "-9.999999999999999e-6"},
            {0x3ee4f8b588e368f2ULL, "1.0000000000000003e-5"},
            {0xbf1a36e2eb1c432eULL, "-0.00010000000000000002"},
            {0x3f1a36e2eb1c432cULL, "9.999999999999999e-5"},
            {0xbf1a36e2eb1c432cULL, "-9.999999999999999e-5"},
            {0x3f1a36e2eb1c432eULL, "0.00010000000000000002"},
            {0xc341c37937e08001ULL, "-10000000000000002.0"},
            {0x4341c37937e07fffULL, "9999999999999998.0"},
            {0xc341c37937e07fffULL, "-9999999999999998.0"},
            {0x4341c37937e08001ULL, "10000000000000002.0"},
            {0xc376345785d8a001ULL, "-1.0000000000000002e+17"},
            {0x4376345785d89fffULL, "99999999999999980.0"},
            {0xc376345785d89fffULL, "-99999999999999980.0"},
            {0x4376345785d8a001ULL, "1.0000000000000002e+17"},
            {0xc4b52d02c7e14af8ULL, "-1.0000000000000003e+23"},
            {0x44b52d02c7e14af6ULL, "1.0e+23"},
            {0xc4b52d02c7e14af6ULL, "-1.0e+23"},
            {0x44b52d02c7e14af8ULL, "1.0000000000000003e+23"}}) {
            test::require(canonical::encode(canonical::value(canonical::binary64{bits})) == expected,
                          "binary64 PHP oracle adjacent-ULP regression");
        }
        canonical::sha256 empty;
        test::require(empty.finish() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 empty KAT");
        canonical::sha256 multi;
        for (int i = 0; i < 1000; ++i) multi.update(std::string(1000, 'a'));
        test::require(multi.finish() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "SHA256 million-a chunked KAT");
        canonical::sha256 abc;
        abc.update("a"); abc.update("bc");
        test::require(abc.finish() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 abc KAT");
        canonical::sha256 boundary;
        boundary.update("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
        test::require(boundary.finish() == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "SHA256 two-block padding KAT");
        const auto request = json::parse(R"({"profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"array","entries":[{"key":{"type":"int","decimal":"1"},"value":{"type":"string","base64":"b25l"}},{"key":{"type":"int","decimal":"0"},"value":{"type":"string","base64":"emVybw=="}}]}})");
        test::require(canonical::evaluate(request).at("output").as<std::string>() == "[\"zero\",\"one\"]", "tagged bridge preserves input key order and reclassification");
        for (const auto* bad : {
            R"({"profile":"other","operation":"encode","input":{"type":"null"}})",
            R"({"profile":"kumwe-canonical-json/generic-v1","operation":"other","input":{"type":"null"}})",
            R"({"profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"int","decimal":"01"}})",
            R"({"profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"array","entries":[{"key":{"type":"string","base64":"MQ=="},"value":{"type":"null"}}]}})",
            R"({"profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"array","entries":[{"key":{"type":"int","decimal":"1"},"value":{"type":"null"}},{"key":{"type":"int","decimal":"1"},"value":{"type":"null"}}]}})",
            R"({"profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"string","base64":"/x=="}})"}) {
            bool malformed = false;
            try { (void)canonical::evaluate(json::parse(bad)); } catch (const std::invalid_argument&) { malformed = true; }
            test::require(malformed, "malformed transport refused without profile coercion");
        }
        for (const auto* bad : {"{\"maxDepth\":65}", "{\"maxNodes\":0}", "{\"maxOutputBytes\":8388609}",
                                "{\"maxInputBytes\":16777217}", "{\"unknown\":1}", "{\"maxDepth\":true}"}) {
            bool malformed = false;
            try { (void)canonical::limits_from_json(json::parse(bad)); } catch (const std::invalid_argument&) { malformed = true; }
            test::require(malformed, "profile ceilings cannot be raised");
        }
        for (const auto& [wire, expected] : std::vector<std::pair<std::string, std::uint32_t>>{
            {R"({"wire_version":1,"corpus_digest":"wrong","profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"null"}})", KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS},
            {R"({"wire_version":2,"corpus_digest":"84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250","profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"null"}})", KUMWE_ENGINE_V1_UNSUPPORTED_VERSION},
            {R"({"wire_version":1,"corpus_digest":"84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250","profile":"kumwe-canonical-json/generic-v1","operation":"encode","input":{"type":"bad"}})", KUMWE_ENGINE_V1_INVALID_INPUT}}) {
            const auto view = test::view(wire);
            test::response owner;
            test::require(kumwe_engine_v1_canonical(&view, &owner.buffer) == expected, "C ABI refuses malformed or incompatible envelope");
            test::require(owner.buffer == nullptr, "C ABI refusal publishes no partial result");
        }
        canonical::value fresh(std::string("ok"));
        canonical::limits tiny; tiny.max_output_bytes = 1;
        bool refused = false;
        try { (void)canonical::digest(fresh, tiny); } catch (const canonical::error&) { refused = true; }
        test::require(refused && canonical::encode(fresh) == "\"ok\"", "refusal retains no partial operation state");
        std::cout << count << " canonical vectors passed for bytes and streaming SHA256; KAT and 74 C ABI/bridge/reuse checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
