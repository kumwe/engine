#include "support.hpp"
#include "value/json.hpp"
#include "batch.hpp"
#include <iostream>
#include <limits>
#include <utility>
#include <vector>
int main() {
    using namespace std::string_literals;
    using namespace kumwe::engine;
    try {
        const auto object = json::parse("{\"z\":9223372036854775807,\"a\":[-9223372036854775808,\"\\ud83d\\ude00\",true,null]}");
        test::require(object.at("z").as<std::int64_t>() == std::numeric_limits<std::int64_t>::max(), "int64 upper bound");
        test::require(json::parse(json::encode(object)) == object, "lossless round trip");
        test::require(json::parse("9223372036854775808").is<json::number>(), "out-of-range integer stays lossless");
        test::require(json::parse("1.0").is<json::number>(), "fraction is not an integer");
        for (const std::string_view invalid : {"{\"a\":1,\"a\":2}","[1,]","01","+1","1.","1e+","\"\\ud800\"","null trailing","[","\"\xff\""}) {
            bool refused = false; try { (void)json::parse(invalid); } catch (const refusal&) { refused = true; }
            test::require(refused, "hostile JSON refused");
        }
        for (unsigned constraint = 0; constraint < 4; ++constraint) {
            bool refused = false;
            try {
                if (constraint == 0) (void)json::parse("null", 3);
                if (constraint == 1) (void)json::parse("[null]", 100, 1);
                if (constraint == 2) (void)json::parse("[null]", 100, 2, 0);
                if (constraint == 3) (void)json::encode(json::value("é"), 3);
            } catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "resource budget enforced");
        }
        test::require(json::encode(json::value("\0\n\t\xe2\x80\xa8"s)) == "\"\\u0000\\n\\t\\u2028\"", "PHP-compatible UTF8 escaping");
        for (const auto& [input, expected] : std::vector<std::pair<std::string, std::string>>{
            {"ordinary UTF8 é界", "\"ordinary UTF8 é界\""},
            {"prefix\"middle\\suffix", "\"prefix\\\"middle\\\\suffix\""},
            {"\"edge\"", "\"\\\"edge\\\"\""},
            {"x\0\x1f\b\f\n\r\tend"s, "\"x\\u0000\\u001f\\b\\f\\n\\r\\tend\""},
            {"before\xe2\x80\xa8" "between\xe2\x80\xa9"s, "\"before\\u2028between\\u2029\""},
            {std::string(4096, 'a') + '"' + std::string(4096, 'b'),
                '"' + std::string(4096, 'a') + "\\\"" + std::string(4096, 'b') + '"'}}) {
            test::require(json::encode(json::value(input), expected.size()) == expected, "exact UTF8 text spans and escape bytes");
            test::require(json::parse(expected).as<std::string>() == input, "parser retains text spans around every escape kind");
            test::require(json::encoded_size(json::value(input), expected.size()) == expected.size(), "counting writer uses exact UTF8 escape lengths");
            bool refused = false;
            try { (void)json::encode(json::value(input), expected.size() - 1); }
            catch (const refusal& failure) { refused = failure.code == KUMWE_ENGINE_V1_EXHAUSTED_LIMIT; }
            test::require(refused, "text spans retain exact output limit");
        }
        for (const auto& source : std::vector<std::string>{
            '\"' + std::string(8192, 'a'),
            '\"' + std::string(8192, 'a') + '\\',
            '\"' + std::string(8192, 'a') + '\x1f' + '\"',
            '\"' + std::string(8192, 'a') + '\xff' + '\"',
            '\"' + std::string(8192, 'a') + "\\ud800tail\"",
            '\"' + std::string(8192, 'a') + "\\q\""}) {
            bool refused = false;
            try { (void)json::parse(source); }
            catch (const refusal& error) { refused = error.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
            test::require(refused, "parser refuses malformed input after a long ordinary span");
        }
        bool invalid_before_limit = false;
        try { (void)json::encode(json::value(std::string(4096, 'a') + '\xff'), 1); }
        catch (const refusal& failure) { invalid_before_limit = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(invalid_before_limit, "UTF8 refusal still precedes string output limit");
        invalid_before_limit = false;
        try { (void)json::encoded_size(json::value(std::string(4096, 'a') + '\xff'), 1); }
        catch (const refusal& failure) { invalid_before_limit = failure.code == KUMWE_ENGINE_V1_INVALID_INPUT; }
        test::require(invalid_before_limit, "counting writer retains UTF8 refusal precedence");
        for (const auto& input : std::vector<json::value>{
            json::parse(R"({"nested":[null,false,true,-9223372036854775808,1.2300e-1000,9223372036854775808,{"é":"a\n\"b"}],"empty":{}})"),
            json::value("\0\"\xe2\x80\xa8界"s), json::value(json::value::list{})}) {
            const auto encoded = json::encode(input);
            const auto measured = json::encode_with_quoted_size(input);
            test::require(measured.bytes == encoded
                && measured.quoted_size == json::encoded_size(json::value(encoded)),
                "fused serialization retains exact quoted transport size");
            for (std::size_t limit = 0; limit <= encoded.size() + 1; ++limit) {
                std::uint32_t written_status = 0, counted_status = 0;
                std::size_t written = 0, counted = 0;
                try { written = json::encode(input, limit).size(); }
                catch (const refusal& failure) { written_status = failure.code; }
                try { counted = json::encoded_size(input, limit); }
                catch (const refusal& failure) { counted_status = failure.code; }
                test::require(written_status == counted_status && written == counted,
                    "counting and materializing writers have identical nested/numeric byte boundaries");
            }
        }
        std::string controls;
        for (unsigned i = 0; i < 32; ++i) controls.push_back(static_cast<char>(i));
        for (const auto& input : std::vector<json::value>{
            json::value(json::value::object{}), json::value(json::value::list{}),
            json::value(std::numeric_limits<std::int64_t>::min()),
            json::value(std::numeric_limits<std::int64_t>::max()),
            json::value(controls + "\"\\é界\xe2\x80\xa8\xe2\x80\xa9"),
            json::value(json::value::object{{controls + "\"\\é", json::value(controls)},
                {"nested", json::parse(R"([{},[],1.2300e-1000,9223372036854775808,{"quote\"":"slash\\"}])")}})}) {
            const auto encoded = json::encode(input);
            const auto measured = json::encode_with_quoted_size(input, encoded.size());
            test::require(measured.bytes == encoded
                && measured.quoted_size == json::encode(json::value(encoded)).size(),
                "fused quote accounting covers all control/UTF8/key/numeric/container kinds");
            for (std::size_t limit = 0; limit <= encoded.size() + 1; ++limit) {
                std::uint32_t written_status = 0, counted_status = 0, measured_status = 0;
                try { (void)json::encode(input, limit); } catch (const refusal& e) { written_status = e.code; }
                try { (void)json::encoded_size(input, limit); } catch (const refusal& e) { counted_status = e.code; }
                try { (void)json::encode_with_quoted_size(input, limit); } catch (const refusal& e) { measured_status = e.code; }
                test::require(written_status == counted_status && written_status == measured_status,
                    "fused writers preserve every byte-limit boundary");
            }
        }
        for (const auto& input : std::vector<json::value>{
            json::value(controls + std::string(4096, 'a') + '\xff'),
            json::value(json::value::list{json::value("first"), json::value("\xed\xa0\x80")}),
            json::value(json::value::object{{"a", json::value("first")}, {"z", json::value("\xf4\x90\x80\x80")}})}) {
            for (const std::size_t limit : {0U, 1U, 8U, 32U, 65536U}) {
                std::uint32_t written_status = 0, counted_status = 0, measured_status = 0;
                try { (void)json::encode(input, limit); } catch (const refusal& e) { written_status = e.code; }
                try { (void)json::encoded_size(input, limit); } catch (const refusal& e) { counted_status = e.code; }
                try { (void)json::encode_with_quoted_size(input, limit); } catch (const refusal& e) { measured_status = e.code; }
                test::require(written_status != 0 && written_status == counted_status && written_status == measured_status,
                    "late invalid UTF8 and prior-container byte refusal keep ordered precedence");
            }
        }
        std::cout << "Lossless transport and hostile-input tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
