// Generate only the pinned Unicode properties needed by native field normalization.
// Port of the former tools/generate-unicode-data.py; the emitted header is byte-identical
// apart from the first comment line naming this file.
#include "canonical/sha256.hpp"
#include "value/json.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using code_point = std::uint32_t;
using sequence = std::vector<code_point>;
using mapping_table = std::map<code_point, sequence>;
using range_list = std::vector<std::pair<code_point, code_point>>;
using row_type = std::vector<std::string>;

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }
void require(bool condition, const char* what) {
    if (!condition) fail(std::string("Assertion failed: ") + what);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) fail("Cannot read " + path.string());
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad()) fail("Cannot read " + path.string());
    return contents;
}

// Python str.isspace for the ASCII range: the pinned inputs are ASCII outside comments.
bool is_space(char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    return byte == ' ' || (byte >= '\t' && byte <= '\r') || (byte >= 0x1c && byte <= 0x1f);
}
// Python str.splitlines boundaries after universal-newline translation (\r\n and \r become \n).
bool is_line_break(char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    return byte == '\n' || byte == '\r' || byte == '\v' || byte == '\f' || (byte >= 0x1c && byte <= 0x1e);
}

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
    while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
    return text;
}

// Python str.split() without arguments: runs of whitespace separate words, no empty words.
std::vector<std::string_view> split_words(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && is_space(text[index])) ++index;
        const std::size_t start = index;
        while (index < text.size() && !is_space(text[index])) ++index;
        if (index > start) words.push_back(text.substr(start, index - start));
    }
    return words;
}

// Python str.split(separator): every piece is kept, including empty ones.
std::vector<std::string_view> split_on(std::string_view text, std::string_view separator) {
    std::vector<std::string_view> pieces;
    std::size_t start = 0;
    while (true) {
        const std::size_t found = text.find(separator, start);
        if (found == std::string_view::npos) {
            pieces.push_back(text.substr(start));
            return pieces;
        }
        pieces.push_back(text.substr(start, found - start));
        start = found + separator.size();
    }
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (!is_line_break(text[index])) continue;
        lines.push_back(text.substr(start, index - start));
        if (text[index] == '\r' && index + 1 < text.size() && text[index + 1] == '\n') ++index;
        start = index + 1;
    }
    if (start < text.size()) lines.push_back(text.substr(start));
    return lines;
}

std::vector<row_type> records(const std::filesystem::path& source, const char* version, const char* name) {
    std::vector<row_type> rows;
    const std::string text = read_file(source / version / name);
    for (const std::string_view raw : split_lines(text)) {
        const std::string_view line = trim(raw.substr(0, raw.find('#')));
        if (line.empty()) continue;
        row_type row;
        for (const std::string_view field : split_on(line, ";")) row.emplace_back(trim(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

// Python indexing raises on a short row; keep that as a failure rather than reading past the end.
const std::string& field(const row_type& row, std::size_t index) {
    if (index >= row.size()) fail("Row has fewer than " + std::to_string(index + 1) + " fields");
    return row[index];
}

code_point parse_digits(std::string_view text, unsigned base) {
    if (text.empty()) fail("Empty number");
    std::uint64_t value = 0;
    for (const char c : text) {
        unsigned digit;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a') + 10U;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A') + 10U;
        else fail("Invalid number: " + std::string(text));
        value = value * base + digit;
        if (value > 0xffffffffULL) fail("Number out of range: " + std::string(text));
    }
    return static_cast<code_point>(value);
}
code_point parse_hex(std::string_view text) { return parse_digits(text, 16); }
code_point parse_decimal(std::string_view text) { return parse_digits(text, 10); }

sequence hex_sequence(std::string_view text) {
    sequence values;
    for (const std::string_view word : split_words(text)) values.push_back(parse_hex(word));
    return values;
}

bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

range_list compact(range_list ranges) {
    std::sort(ranges.begin(), ranges.end());
    range_list merged;
    for (const auto& [start, end] : ranges) {
        if (!merged.empty() && start <= merged.back().second + 1) merged.back().second = std::max(end, merged.back().second);
        else merged.emplace_back(start, end);
    }
    return merged;
}

std::size_t longest(const mapping_table& table) {
    if (table.empty()) fail("max() arg is an empty sequence");
    std::size_t result = 0;
    for (const auto& entry : table) result = std::max(result, entry.second.size());
    return result;
}

sequence expand(const mapping_table& decompositions, code_point cp, sequence& path) {
    require(std::find(path.begin(), path.end(), cp) == path.end() && path.size() < 32, "decomposition cycle or depth");
    const auto found = decompositions.find(cp);
    if (found == decompositions.end()) return {cp};
    sequence result;
    path.push_back(cp);
    for (const code_point item : found->second) {
        const sequence part = expand(decompositions, item, path);
        result.insert(result.end(), part.begin(), part.end());
    }
    path.pop_back();
    return result;
}

void emit_mappings(std::string& out, const char* name, const mapping_table& data) {
    out += "inline constexpr mapping ";
    out += name;
    out += "[] = {\n";
    for (const auto& [cp, values] : data) {
        out += "    {" + std::to_string(cp) + ", {";
        for (std::size_t index = 0; index < 4; ++index) {
            if (index) out += ", ";
            out += std::to_string(index < values.size() ? values[index] : 0U);
        }
        out += "}, " + std::to_string(values.size()) + "},\n";
    }
    out += "};\n";
}

void emit_ranges(std::string& out, const char* name, const range_list& ranges) {
    out += "inline constexpr range ";
    out += name;
    out += "[] = {\n";
    for (const auto& [start, end] : ranges) out += "    {" + std::to_string(start) + ", " + std::to_string(end) + "},\n";
    out += "};\n";
}

int run(const std::filesystem::path& root, bool check) {
    namespace json = kumwe::engine::json;
    const std::filesystem::path source = root / "third_party/unicode";
    const json::value manifest = json::parse(read_file(root / "resources/unicode-source.json"));
    for (const auto& [name, expected] : manifest.at("files").as<json::value::object>()) {
        kumwe::engine::canonical::sha256 digest;
        digest.update(read_file(source / name));
        if (!expected.is<std::string>() || digest.finish() != expected.as<std::string>()) {
            std::cerr << "Pinned Unicode source changed: " << name << '\n';
            return 1;
        }
    }

    mapping_table lower, upper;
    for (const row_type& row : records(source, "17.0.0", "UnicodeData.txt")) {
        const code_point cp = parse_hex(field(row, 0));
        if (!field(row, 13).empty()) lower[cp] = {parse_hex(field(row, 13))};
        if (!field(row, 12).empty()) upper[cp] = {parse_hex(field(row, 12))};
    }
    for (const row_type& row : records(source, "17.0.0", "SpecialCasing.txt")) {
        if (!field(row, 4).empty()) {
            // Default casing has exactly one language-independent context rule.
            bool language_specific = false;
            for (const std::string_view word : split_words(row[4]))
                if (word == "lt" || word == "tr" || word == "az") language_specific = true;
            if (!language_specific)
                require(row[0] == "03A3" && row[1] == "03C2" && row[2] == "03A3" && row[3] == "03A3" && row[4] == "Final_Sigma",
                        "unexpected conditional SpecialCasing row");
            continue;
        }
        const code_point cp = parse_hex(field(row, 0));
        lower[cp] = hex_sequence(field(row, 1));
        upper[cp] = hex_sequence(field(row, 3));
    }
    for (mapping_table* mapping : {&lower, &upper})
        std::erase_if(*mapping, [](const auto& entry) { return entry.second == sequence{entry.first}; });

    range_list cased, case_ignorable;
    for (const row_type& row : records(source, "17.0.0", "DerivedCoreProperties.txt")) {
        const std::string& property = field(row, 1);
        range_list* target = nullptr;
        if (property == "Cased") target = &cased;
        else if (property == "Case_Ignorable") target = &case_ignorable;
        if (!target) continue;
        sequence limits;
        for (const std::string_view part : split_on(row[0], "..")) limits.push_back(parse_hex(part));
        target->emplace_back(limits.front(), limits.back());
    }
    cased = compact(std::move(cased));
    case_ignorable = compact(std::move(case_ignorable));

    std::map<code_point, std::uint32_t> classes;
    mapping_table decompositions;
    for (const row_type& row : records(source, "15.1.0", "UnicodeData.txt")) {
        const code_point cp = parse_hex(field(row, 0));
        const std::uint32_t combining_class = parse_decimal(field(row, 3));
        if (combining_class) {
            require(!ends_with(row[1], "First>") && !ends_with(row[1], "Last>"), "combining class on a range marker");
            classes[cp] = combining_class;
        }
        const std::string& decomposition = field(row, 5);
        if (!decomposition.empty() && decomposition.front() != '<') decompositions[cp] = hex_sequence(decomposition);
    }
    std::set<code_point> excluded;
    for (const row_type& row : records(source, "15.1.0", "CompositionExclusions.txt")) excluded.insert(parse_hex(field(row, 0)));
    std::map<std::uint64_t, code_point> compositions;
    for (const auto& [cp, decomposition] : decompositions) {
        // Full_Composition_Exclusion also includes singleton and non-starter decompositions.
        if (excluded.count(cp) || decomposition.size() != 2) continue;
        const auto starter = classes.find(decomposition[0]);
        if (starter != classes.end() && starter->second != 0) continue;
        const std::uint64_t key = (static_cast<std::uint64_t>(decomposition[0]) << 21) | decomposition[1];
        require(!compositions.count(key), "duplicate composition pair");
        compositions[key] = cp;
    }

    mapping_table decomposed;
    for (const auto& entry : decompositions) {
        sequence path;
        decomposed[entry.first] = expand(decompositions, entry.first, path);
    }
    require(longest(decomposed) <= 4, "full decomposition longer than 4");
    require(longest(lower) <= 3 && longest(upper) <= 3, "case mapping longer than 3");

    std::string generated =
        "// Generated by tools/generate-unicode-data.cpp from pinned Unicode data. Do not edit.\n"
        "// Unicode License V3: third_party/unicode/LICENSE.\n"
        "#ifndef KUMWE_ENGINE_UNICODE_DATA_HPP\n"
        "#define KUMWE_ENGINE_UNICODE_DATA_HPP\n"
        "#include <cstdint>\n"
        "#include <cstddef>\n"
        "namespace kumwe::engine::document::unicode_data {\n"
        "struct mapping { std::uint32_t point; std::uint32_t values[4]; std::uint8_t size; };\n"
        "struct range { std::uint32_t first; std::uint32_t last; };\n"
        "struct combining { std::uint32_t point; std::uint8_t value; };\n"
        "struct composition { std::uint64_t pair; std::uint32_t point; };\n";
    emit_mappings(generated, "lowercase", lower);
    emit_mappings(generated, "uppercase", upper);
    emit_mappings(generated, "decomposition", decomposed);
    emit_ranges(generated, "cased", cased);
    emit_ranges(generated, "case_ignorable", case_ignorable);
    generated += "inline constexpr combining classes[] = {\n";
    for (const auto& [cp, value] : classes) generated += "    {" + std::to_string(cp) + ", " + std::to_string(value) + "},\n";
    generated += "};\ninline constexpr composition compositions[] = {\n";
    for (const auto& [pair, cp] : compositions) generated += "    {" + std::to_string(pair) + "ULL, " + std::to_string(cp) + "},\n";
    generated += "};\n}\n#endif\n";

    const std::filesystem::path target = root / "src/document/unicode_data.hpp";
    if (check) {
        if (read_file(target) != generated) {
            std::cerr << "Generated Unicode tables differ from pinned normative inputs.\n";
            return 1;
        }
    } else {
        std::ofstream file(target, std::ios::binary | std::ios::trunc);
        if (!file || !(file << generated) || !file.flush()) fail("Cannot write " + target.string());
    }
    std::cout << "Unicode 17 casing / 15.1 NFC tables verified: " << lower.size() << " lower, " << upper.size()
              << " upper, " << decomposed.size() << " decompositions, " << compositions.size() << " compositions.\n";
    return 0;
}
}

int main(int argc, char** argv) {
    const bool check = argc == 3 && std::string_view(argv[2]) == "--check";
    if (argc != 2 && !check) {
        std::cerr << "Usage: generate-unicode-data ROOT [--check]\n";
        return 2;
    }
    try {
        return run(std::filesystem::path(argv[1]), check);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unicode data generation failed with a non-standard exception.\n";
        return 1;
    }
}
