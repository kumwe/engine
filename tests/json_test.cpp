#include "support.hpp"
#include "value/json.hpp"
#include "batch.hpp"
#include <iostream>
#include <limits>
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
        std::cout << "Lossless transport and hostile-input tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
