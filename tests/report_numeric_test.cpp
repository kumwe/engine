#include "support.hpp"
#include "value/php_numeric.hpp"
#include "value/json.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace kumwe::engine;
    using value = json::value;
    try {
        test::require(argc == 2,"numeric comparison corpus path required");
        std::ifstream stream(argv[1],std::ios::binary);
        test::require(stream.good(),"numeric comparison corpus readable");
        const std::string bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        const auto corpus = json::parse(bytes);
        std::size_t count = 0;
        for (const auto& pair : corpus.at("vectors").as<value::list>()) {
            const auto& left = pair.at("left").as<std::string>();
            const auto& right = pair.at("right").as<std::string>();
            const int actual = value_compat::php_string_compare(left,right);
            if (actual != pair.at("expected").as<std::int64_t>()) {
                std::cerr << "numeric comparison " << json::encode(pair) << " actual " << actual << '\n';
                return 1;
            }
            ++count;
        }
        test::require(count >= 3000,"full numeric comparison cross-product replayed");
        std::cout << count << " frozen PHP numeric string comparisons passed\n";
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
