#include "canonical/canonical.hpp"
#include "canonical/sha256.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <limits>

namespace kumwe::engine::canonical {
namespace {
[[noreturn]] void reject(std::string_view code) { throw error(std::string(code)); }
[[noreturn]] void malformed() { throw std::invalid_argument("Malformed canonical tagged input."); }
void validate_limits(const limits& b) {
    if (b.max_depth > 64 || b.max_nodes == 0 || b.max_nodes > 100000
        || b.max_output_bytes == 0 || b.max_output_bytes > 8388608
        || b.max_input_bytes == 0 || b.max_input_bytes > 16777216) malformed();
}
std::string integer_text(std::int64_t number) {
    std::array<char, 24> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
    if (result.ec != std::errc{}) malformed();
    return {buffer.data(), result.ptr};
}
std::int64_t parse_integer(std::string_view source) {
    std::int64_t number = 0;
    const auto result = std::from_chars(source.data(), source.data() + source.size(), number);
    if (result.ec != std::errc{} || result.ptr != source.data() + source.size() || integer_text(number) != source) malformed();
    return number;
}
bool coercing_key(std::string_view source) {
    if (source.empty() || source.front() == '+' || source == "-0") return false;
    std::int64_t number = 0;
    const auto result = std::from_chars(source.data(), source.data() + source.size(), number);
    return result.ec == std::errc{} && result.ptr == source.data() + source.size() && integer_text(number) == source;
}
std::string float_text(binary64 source) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    const auto number = std::bit_cast<double>(source.bits);
    if ((source.bits & 0x7fffffffffffffffULL) == 0) return (source.bits >> 63U) != 0 ? "-0.0" : "0.0";
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number, std::chars_format::scientific);
    if (result.ec != std::errc{}) malformed();
    const std::string shortest(buffer.data(), result.ptr);
    const auto e = shortest.find('e');
    if (e == std::string::npos) malformed();
    const bool negative = shortest.front() == '-';
    const std::size_t start = negative ? 1U : 0U;
    std::string digits;
    for (std::size_t i = start; i < e; ++i) if (shortest[i] != '.') digits += shortest[i];
    const auto exponent_text = std::string_view(shortest).substr(e + 1);
    const bool positive_exponent = exponent_text.front() == '+';
    const auto exponent_digits = positive_exponent ? exponent_text.substr(1) : exponent_text;
    int exponent = 0;
    const auto parsed = std::from_chars(exponent_digits.data(), exponent_digits.data() + exponent_digits.size(), exponent);
    if (parsed.ec != std::errc{} || parsed.ptr != exponent_digits.data() + exponent_digits.size()) malformed();
    std::string output = negative ? "-" : "";
    if (exponent >= -4 && exponent <= 16) {
        const int point = exponent + 1;
        if (point <= 0) {
            output += "0.";
            output.append(static_cast<std::size_t>(-point), '0');
            output += digits;
        } else if (static_cast<std::size_t>(point) >= digits.size()) {
            output += digits;
            output.append(static_cast<std::size_t>(point) - digits.size(), '0');
            output += ".0";
        } else {
            output += digits.substr(0, static_cast<std::size_t>(point));
            output += '.';
            output += digits.substr(static_cast<std::size_t>(point));
        }
    } else {
        output += digits.front();
        output += '.';
        output += digits.size() == 1 ? "0" : digits.substr(1);
        output += exponent >= 0 ? "e+" : "e";
        output += integer_text(exponent);
    }
    return output;
}
struct normalized final {
    const value* source;
    // vector permits an incomplete element type; pair<string, normalized> does
    // not on every supported C++20 standard library.
    std::vector<normalized> entries;
    bool list = false;
    std::string key;
};
struct admission final {
    limits bounds;
    std::size_t nodes = 0, bytes = 0;
    void admit(std::size_t count) {
        if (count > bounds.max_input_bytes - bytes) reject("canonical.input-limit");
        bytes += count;
    }
    normalized visit(const value& source, std::size_t depth) {
        if (depth > bounds.max_depth) reject("canonical.depth-limit");
        if (nodes == bounds.max_nodes) reject("canonical.node-limit");
        ++nodes;
        normalized result{&source, {}, false, {}};
        if (const auto* members = std::get_if<value::array>(&source.data)) {
            if (members->size() > bounds.max_nodes - nodes) reject("canonical.node-limit");
            // Admit every immediate key before creating sort indices or copying key bytes.
            for (const auto& [key, child] : *members) {
                (void)child;
                const auto* text = std::get_if<std::string>(&key);
                admit(text == nullptr ? 8 : text->size());
            }
            struct entry final { const value* source; const value::key* key; std::string text; };
            std::vector<entry> ordered;
            ordered.reserve(members->size());
            bool initially_list = true;
            std::size_t index = 0;
            for (const auto& [key, child] : *members) {
                const auto* integer = std::get_if<std::int64_t>(&key);
                if (integer == nullptr || *integer != static_cast<std::int64_t>(index)) initially_list = false;
                auto text = integer != nullptr ? integer_text(*integer) : std::get<std::string>(key);
                if (integer == nullptr && coercing_key(text)) malformed();
                ordered.push_back({&child, &key, std::move(text)});
                ++index;
            }
            if (!initially_list) {
                std::sort(ordered.begin(), ordered.end(), [](const entry& a, const entry& b) {
                    return std::lexicographical_compare(a.text.begin(), a.text.end(), b.text.begin(), b.text.end(),
                        [](char x, char y) { return static_cast<unsigned char>(x) < static_cast<unsigned char>(y); });
                });
                for (std::size_t i = 1; i < ordered.size(); ++i) if (ordered[i - 1].text == ordered[i].text) malformed();
            }
            result.list = true;
            result.entries.reserve(ordered.size());
            index = 0;
            for (const auto& item : ordered) {
                const auto* integer = std::get_if<std::int64_t>(item.key);
                if (integer == nullptr || *integer != static_cast<std::int64_t>(index)) result.list = false;
                auto child = visit(*item.source, depth + 1);
                child.key = item.text;
                result.entries.push_back(std::move(child));
                ++index;
            }
            return result;
        }
        if (const auto* text = std::get_if<std::string>(&source.data)) admit(text->size());
        else if (std::holds_alternative<std::int64_t>(source.data) || std::holds_alternative<binary64>(source.data)) admit(8);
        else if (std::holds_alternative<bool>(source.data)) admit(1);
        if (std::holds_alternative<unsupported>(source.data)) reject("canonical.unsupported-type");
        if (const auto* number = std::get_if<binary64>(&source.data)) {
            if ((number->bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL) reject("canonical.non-finite-number");
        }
        return result;
    }
};
struct sink final {
    std::size_t maximum, used = 0;
    std::string* output;
    sha256* hash;
    void token(std::string_view bytes) {
        if (bytes.size() > maximum - used) reject("canonical.output-limit");
        used += bytes.size();
        if (output != nullptr) output->append(bytes);
        if (hash != nullptr) hash->update(bytes);
    }
    void quoted(std::string_view bytes) {
        if (!json::valid_utf8(bytes)) reject("canonical.invalid-utf8");
        token("\"");
        std::size_t begin = 0;
        constexpr std::string_view hex = "0123456789abcdef";
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto byte = static_cast<unsigned char>(bytes[i]);
            std::string_view escaped;
            std::array<char, 6> control{'\\', 'u', '0', '0', '0', '0'};
            std::size_t width = 1;
            switch (byte) {
                case '"': escaped = "\\\""; break;
                case '\\': escaped = "\\\\"; break;
                case '\b': escaped = "\\b"; break;
                case '\f': escaped = "\\f"; break;
                case '\n': escaped = "\\n"; break;
                case '\r': escaped = "\\r"; break;
                case '\t': escaped = "\\t"; break;
                default:
                    if (byte < 32) {
                        control[4] = hex[byte >> 4U]; control[5] = hex[byte & 15U];
                        escaped = std::string_view(control.data(), control.size());
                    } else if (byte == 0xe2U && i + 2 < bytes.size()
                        && static_cast<unsigned char>(bytes[i + 1]) == 0x80U
                        && (static_cast<unsigned char>(bytes[i + 2]) == 0xa8U || static_cast<unsigned char>(bytes[i + 2]) == 0xa9U)) {
                        escaped = static_cast<unsigned char>(bytes[i + 2]) == 0xa8U ? "\\u2028" : "\\u2029";
                        width = 3;
                    }
            }
            if (!escaped.empty()) {
                token(bytes.substr(begin, i - begin));
                token(escaped);
                i += width - 1;
                begin = i + 1;
            }
        }
        token(bytes.substr(begin));
        token("\"");
    }
    void emit(const normalized& input) {
        const auto& source = input.source->data;
        if (std::holds_alternative<value::array>(source)) {
            token(input.list ? "[" : "{");
            bool first = true;
            for (const auto& child : input.entries) {
                if (!first) token(",");
                first = false;
                if (!input.list) { quoted(child.key); token(":"); }
                emit(child);
            }
            token(input.list ? "]" : "}");
        } else if (const auto* text = std::get_if<std::string>(&source)) quoted(*text);
        else if (const auto* number = std::get_if<std::int64_t>(&source)) token(integer_text(*number));
        else if (const auto* number = std::get_if<binary64>(&source)) token(float_text(*number));
        else if (const auto* boolean = std::get_if<bool>(&source)) token(*boolean ? "true" : "false");
        else token("null");
    }
    // decode(..., semantics) has already admitted every value and sorted each
    // array in the normative traversal order. Re-admission used to allocate a
    // second tree and sort/copy every key again at the coarse ABI boundary.
    void emit_admitted(const value& input) {
        const auto* members = std::get_if<value::array>(&input.data);
        if (members == nullptr) {
            emit(normalized{&input, {}, false, {}});
            return;
        }
        bool list = true;
        std::size_t index = 0;
        for (const auto& [key, child] : *members) {
            (void)child;
            const auto* integer = std::get_if<std::int64_t>(&key);
            if (integer == nullptr || *integer != static_cast<std::int64_t>(index++)) {
                list = false; break;
            }
        }
        token(list ? "[" : "{");
        bool first = true;
        for (const auto& [key, child] : *members) {
            if (!first) token(",");
            first = false;
            if (!list) {
                if (const auto* integer = std::get_if<std::int64_t>(&key)) quoted(integer_text(*integer));
                else quoted(std::get<std::string>(key));
                token(":");
            }
            emit_admitted(child);
        }
        token(list ? "]" : "}");
    }
};
std::string unbase64(std::string_view input) {
    static constexpr auto alphabet = [] {
        std::array<unsigned char, 256> lookup{};
        lookup.fill(255);
        constexpr std::string_view symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::size_t i = 0; i < symbols.size(); ++i)
            lookup[static_cast<unsigned char>(symbols[i])] = static_cast<unsigned char>(i);
        return lookup;
    }();
    if (input.size() % 4 != 0 || input.size() > 22369624) malformed();
    std::string output;
    output.reserve(input.size() / 4 * 3);
    for (std::size_t i = 0; i < input.size(); i += 4) {
        std::uint32_t bits = 0;
        unsigned padding = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const char c = input[i + j];
            if (c == '=') {
                if (i + 4 != input.size() || j < 2) malformed();
                ++padding; bits <<= 6U;
            } else {
                if (padding != 0) malformed();
                const auto position = alphabet[static_cast<unsigned char>(c)];
                if (position == 255) malformed();
                bits = (bits << 6U) | static_cast<std::uint32_t>(position);
            }
        }
        if (padding > 2 || (padding == 2 && (bits & 0xffffU) != 0) || (padding == 1 && (bits & 0xffU) != 0)) malformed();
        output += static_cast<char>((bits >> 16U) & 0xffU);
        if (padding < 2) output += static_cast<char>((bits >> 8U) & 0xffU);
        if (padding == 0) output += static_cast<char>(bits & 0xffU);
    }
    return output;
}
const std::string& string_member(const json::value& input, std::string_view member) {
    const auto* found = input.find(member);
    if (found == nullptr || !found->is<std::string>()) malformed();
    return found->as<std::string>();
}
value decode(const json::value& input, std::size_t depth, std::size_t& nodes, std::size_t& bytes,
             const limits* semantics = nullptr) {
    const auto max_depth = semantics == nullptr ? 65U : semantics->max_depth;
    const auto max_nodes = semantics == nullptr ? 100001U : semantics->max_nodes;
    const auto max_bytes = semantics == nullptr ? 16777216U : semantics->max_input_bytes;
    if (depth > max_depth) reject("canonical.depth-limit");
    if (nodes == max_nodes) reject("canonical.node-limit");
    ++nodes;
    const auto& type = string_member(input, "type");
    auto charge = [&](std::size_t width) {
        if (width > max_bytes - bytes) reject("canonical.input-limit");
        bytes += width;
    };
    auto raw_width = [&](std::string_view encoded) -> std::size_t {
        if (encoded.empty()) return 0;
        if (encoded.size() % 4 != 0) malformed();
        std::size_t padding = encoded.back() == '=' ? 1U : 0U;
        if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') ++padding;
        return encoded.size() / 4 * 3 - padding;
    };
    if (type == "null") return value();
    if (type == "unsupported") {
        if (semantics != nullptr) reject("canonical.unsupported-type");
        return value(unsupported{});
    }
    if (type == "bool") {
        charge(1);
        const auto* item = input.find("value");
        if (item == nullptr || !item->is<bool>()) malformed();
        return value(item->as<bool>());
    }
    if (type == "int") { charge(8); return value(parse_integer(string_member(input, "decimal"))); }
    if (type == "float") {
        charge(8);
        const auto& hex = string_member(input, "hex");
        if (hex.size() != 16) malformed();
        std::uint64_t bits = 0;
        const auto result = std::from_chars(hex.data(), hex.data() + hex.size(), bits, 16);
        if (result.ec != std::errc{} || result.ptr != hex.data() + hex.size()) malformed();
        if (semantics != nullptr && (bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL) {
            reject("canonical.non-finite-number");
        }
        return value(binary64{bits});
    }
    if (type == "string") {
        const auto& encoded = string_member(input, "base64");
        charge(raw_width(encoded));
        return value(unbase64(encoded));
    }
    if (type != "array") malformed();
    const auto* entries = input.find("entries");
    if (entries == nullptr || !entries->is<json::value::list>()) malformed();
    const auto& items = entries->as<json::value::list>();
    if (items.size() > max_nodes - nodes) reject("canonical.node-limit");
    // All key admission precedes decoding, ordering and child traversal. Profile limits are
    // applied before expansion allocates any caller-sized native string or entry collection.
    for (const auto& entry : items) {
        const auto& key = entry.at("key");
        const auto& key_type = string_member(key, "type");
        if (key_type == "int") charge(8);
        else if (key_type == "string") charge(raw_width(string_member(key, "base64")));
        else malformed();
    }
    struct tagged_entry final { const json::value* source; value::key key; std::string text; };
    std::vector<tagged_entry> ordered;
    ordered.reserve(items.size());
    std::size_t index = 0;
    bool list = true;
    for (const auto& entry : items) {
        const auto& key = entry.at("key");
        const auto& key_type = string_member(key, "type");
        if (key_type == "int") {
            const auto number = parse_integer(string_member(key, "decimal"));
            if (number != static_cast<std::int64_t>(index)) list = false;
            ordered.push_back({&entry.at("value"), number, integer_text(number)});
        } else {
            auto text = unbase64(string_member(key, "base64"));
            if (coercing_key(text)) malformed();
            list = false;
            ordered.push_back({&entry.at("value"), text, std::move(text)});
        }
        ++index;
    }
    if (semantics != nullptr && !list) {
        std::sort(ordered.begin(), ordered.end(), [](const tagged_entry& a, const tagged_entry& b) {
            return std::lexicographical_compare(a.text.begin(), a.text.end(), b.text.begin(), b.text.end(),
                [](char x, char y) { return static_cast<unsigned char>(x) < static_cast<unsigned char>(y); });
        });
        for (std::size_t i = 1; i < ordered.size(); ++i) if (ordered[i - 1].text == ordered[i].text) malformed();
    }
    value::array result;
    result.reserve(ordered.size());
    for (auto& item : ordered) {
        result.emplace_back(std::move(item.key), decode(*item.source, depth + 1, nodes, bytes, semantics));
    }
    return value(std::move(result));
}
// Frames are tag:u8, payload_bytes:u32le, payload. Array payloads contain a
// u32le count followed by key/value frames. Borrowed frame spans allow semantic
// admission in sorted-key order before allocating caller-sized values.
std::uint64_t little_integer(std::string_view bytes) {
    std::uint64_t output = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        output |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[i])) << (8U * i);
    return output;
}
struct frame final { unsigned char tag; std::string_view payload; };
frame read_frame(std::string_view& input) {
    if (input.size() < 5) malformed();
    const auto tag = static_cast<unsigned char>(input.front());
    const auto size = static_cast<std::size_t>(little_integer(input.substr(1, 4)));
    input.remove_prefix(5);
    if (size > input.size()) malformed();
    const auto payload = input.substr(0, size);
    input.remove_prefix(size);
    return {tag, payload};
}
value decode_binary(frame input, std::size_t depth, std::size_t& nodes,
                    std::size_t& bytes, const limits& bounds) {
    if (depth > bounds.max_depth) reject("canonical.depth-limit");
    if (nodes == bounds.max_nodes) reject("canonical.node-limit");
    ++nodes;
    const auto charge = [&](std::size_t width) {
        if (width > bounds.max_input_bytes - bytes) reject("canonical.input-limit");
        bytes += width;
    };
    if (input.tag <= 2 || input.tag == 7) {
        if (!input.payload.empty()) malformed();
        if (input.tag == 7) reject("canonical.unsupported-type");
        if (input.tag == 0) return value();
        charge(1);
        return value(input.tag == 2);
    }
    if (input.tag == 3 || input.tag == 4) {
        charge(8);
        if (input.payload.size() != 8) malformed();
        const auto bits = little_integer(input.payload);
        if (input.tag == 3) return value(std::bit_cast<std::int64_t>(bits));
        if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL) reject("canonical.non-finite-number");
        return value(binary64{bits});
    }
    if (input.tag == 5) {
        charge(input.payload.size());
        return value(std::string(input.payload));
    }
    if (input.tag != 6 || input.payload.size() < 4) malformed();
    auto remaining = input.payload;
    const auto count = static_cast<std::size_t>(little_integer(remaining.substr(0, 4)));
    remaining.remove_prefix(4);
    if (count > bounds.max_nodes - nodes) reject("canonical.node-limit");
    // Even empty keys/values require two five-byte frames. Check before reserve.
    if (count > remaining.size() / 10) malformed();
    const auto entries = remaining;
    for (std::size_t i = 0; i < count; ++i) {
        const auto key = read_frame(remaining);
        if (key.tag == 3 && key.payload.size() == 8) charge(8);
        else if (key.tag == 5) charge(key.payload.size());
        else malformed();
        (void)read_frame(remaining);
    }
    if (!remaining.empty()) malformed();
    struct entry final { frame child; value::key key; std::string text; };
    std::vector<entry> ordered;
    ordered.reserve(count);
    remaining = entries;
    bool list = true;
    for (std::size_t i = 0; i < count; ++i) {
        const auto key = read_frame(remaining);
        const auto child = read_frame(remaining);
        if (key.tag == 3) {
            const auto number = std::bit_cast<std::int64_t>(little_integer(key.payload));
            if (number != static_cast<std::int64_t>(i)) list = false;
            ordered.push_back({child, number, integer_text(number)});
        } else {
            if (coercing_key(key.payload)) malformed();
            list = false;
            ordered.push_back({child, std::string(key.payload), std::string(key.payload)});
        }
    }
    if (!list) {
        std::sort(ordered.begin(), ordered.end(), [](const entry& a, const entry& b) {
            return std::lexicographical_compare(a.text.begin(), a.text.end(), b.text.begin(), b.text.end(),
                [](char x, char y) { return static_cast<unsigned char>(x) < static_cast<unsigned char>(y); });
        });
        for (std::size_t i = 1; i < ordered.size(); ++i) if (ordered[i - 1].text == ordered[i].text) malformed();
    }
    value::array result;
    result.reserve(count);
    for (auto& item : ordered)
        result.emplace_back(std::move(item.key), decode_binary(item.child, depth + 1, nodes, bytes, bounds));
    return value(std::move(result));
}
}
std::string encode(const value& input, const limits& bounds) {
    validate_limits(bounds);
    admission pass{bounds};
    const auto normalized = pass.visit(input, 0);
    std::string output;
    sink writer{bounds.max_output_bytes, 0, &output, nullptr};
    writer.emit(normalized);
    return output;
}
std::string digest(const value& input, const limits& bounds) {
    validate_limits(bounds);
    admission pass{bounds};
    const auto normalized = pass.visit(input, 0);
    sha256 hash;
    sink writer{bounds.max_output_bytes, 0, nullptr, &hash};
    writer.emit(normalized);
    return hash.finish();
}
value from_tagged(const json::value& input) {
    std::size_t nodes = 0, bytes = 0;
    return decode(input, 0, nodes, bytes);
}
limits limits_from_json(const json::value& input) {
    if (!input.is<json::value::object>()) malformed();
    limits result;
    for (const auto& [name, item] : input.as<json::value::object>()) {
        if (!item.is<std::int64_t>() || item.as<std::int64_t>() < 0) malformed();
        const auto count = static_cast<std::size_t>(item.as<std::int64_t>());
        if (name == "maxDepth") result.max_depth = count;
        else if (name == "maxNodes") result.max_nodes = count;
        else if (name == "maxOutputBytes") result.max_output_bytes = count;
        else if (name == "maxInputBytes") result.max_input_bytes = count;
        else malformed();
    }
    validate_limits(result);
    return result;
}
json::value evaluate(const json::value& request) {
    if (string_member(request, "profile") != "kumwe-canonical-json/generic-v1") malformed();
    const auto& operation = string_member(request, "operation");
    if (operation != "encode" && operation != "digest") malformed();
    const auto* custom = request.find("limits");
    const auto bounds = custom == nullptr ? limits{} : limits_from_json(*custom);
    std::size_t nodes = 0, bytes = 0;
    const auto source = decode(request.at("input"), 0, nodes, bytes, &bounds);
    std::string output;
    sha256 hash;
    sink writer{bounds.max_output_bytes, 0, operation == "encode" ? &output : nullptr,
        operation == "digest" ? &hash : nullptr};
    writer.emit_admitted(source);
    return json::value(json::value::object{{operation == "encode" ? "output" : "sha256",
        json::value(operation == "encode" ? std::move(output) : hash.finish())}});
}
json::value evaluate_binary(const json::value& request, std::string_view input) {
    if (string_member(request, "profile") != "kumwe-canonical-json/generic-v1" || request.find("input") != nullptr) malformed();
    if (!request.is<json::value::object>()) malformed();
    for (const auto& [key, unused] : request.as<json::value::object>()) {
        (void)unused;
        if (key != "profile" && key != "operation" && key != "limits") malformed();
    }
    const auto& operation = string_member(request, "operation");
    if (operation != "encode" && operation != "digest") malformed();
    const auto* custom = request.find("limits");
    const auto bounds = custom == nullptr ? limits{} : limits_from_json(*custom);
    const auto root = read_frame(input);
    if (!input.empty()) malformed();
    std::size_t nodes = 0, bytes = 0;
    const auto source = decode_binary(root, 0, nodes, bytes, bounds);
    std::string output;
    sha256 hash;
    sink writer{bounds.max_output_bytes, 0, operation == "encode" ? &output : nullptr,
        operation == "digest" ? &hash : nullptr};
    writer.emit_admitted(source);
    return json::value(json::value::object{{operation == "encode" ? "output" : "sha256",
        json::value(operation == "encode" ? std::move(output) : hash.finish())}});
}
}
