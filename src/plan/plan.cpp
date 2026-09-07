#include "plan.hpp"
#include "batch.hpp"
#include "build_identity.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <chrono>
#include <set>
#include <type_traits>

namespace kumwe::engine::execution {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
[[noreturn]] void reject(std::uint32_t status = KUMWE_ENGINE_V1_INVALID_INPUT) { throw refusal(status); }
void shape(const value& input, std::initializer_list<std::string_view> allowed) {
    if (!input.is<object>() || input.as<object>().size() != allowed.size()) reject();
    for (const auto& [key, unused] : input.as<object>()) {
        (void)unused;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) reject();
    }
}
const std::string& text(const value& input) {
    if (!input.is<std::string>()) reject();
    return input.as<std::string>();
}
std::uint64_t integer(const value& input, std::uint64_t minimum, std::uint64_t maximum) {
    if (!input.is<std::int64_t>() || input.as<std::int64_t>() < 0) reject();
    const auto number = static_cast<std::uint64_t>(input.as<std::int64_t>());
    if (number < minimum || number > maximum) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    return number;
}
void version(const value& input) {
    if (input != value(std::int64_t{1})) reject(KUMWE_ENGINE_V1_UNSUPPORTED_VERSION);
}
bool token(std::string_view input) {
    if (input.empty() || input.size() > 191) return false;
    for (char c : input) if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')
        && !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.' && c != ':') return false;
    return true;
}
}
plan plan::compile(std::string_view request) {
    const auto started = std::chrono::steady_clock::now();
    const auto envelope = json::parse(request, 67108864);
    const auto* limits = envelope.find("limits");
    if (limits) shape(envelope, {"wire_version", "profile", "corpus_digest", "program", "limits"});
    else shape(envelope, {"wire_version", "profile", "corpus_digest", "program"});
    version(envelope.at("wire_version"));
    const auto profile = text(envelope.at("profile"));
    const auto corpus = text(envelope.at("corpus_digest"));
    // Portable ProgramEnvelope payloads stay opaque through PHP. Native parsing
    // preserves numeric spellings and therefore numeric-kind refusal semantics.
    const auto& payload = envelope.at("program");
    const auto program_bytes = payload.is<std::string>() ? payload.as<std::string>() : json::encode(payload, 16777216);
    std::uint64_t max_input = 16777216, max_output = 16777216, budget = 1000000000, max_ms = 30000;
    if (limits) {
        shape(*limits, {"max_input_bytes", "max_output_bytes", "max_documents", "max_findings", "max_instructions", "max_milliseconds"});
        max_input = integer(limits->at("max_input_bytes"), 1, 67108864);
        max_output = integer(limits->at("max_output_bytes"), 1, 67108864);
        (void)integer(limits->at("max_documents"), 1, 4096);
        (void)integer(limits->at("max_findings"), 1, 65536);
        budget = integer(limits->at("max_instructions"), 1, 1000000000);
        max_ms = integer(limits->at("max_milliseconds"), 1, 600000);
    }
    // Compile work uses conservatively charged source bytes. Profile-specific
    // AST depth/node/collection ceilings also bound every compiler traversal.
    if (program_bytes.size() > max_input || program_bytes.size() > budget) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    const auto program = json::parse(program_bytes, std::min<std::uint64_t>(max_input, 16777216));
    plan output;
    if (profile == "formula-draft/1") {
        if (corpus != KUMWE_ENGINE_FORMULA_CORPUS) reject(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        output.implementation_ = vm::formula::compile(program);
    } else if (profile == "normalized-document-draft/1") {
        if (corpus != KUMWE_ENGINE_DOCUMENT_CORPUS || corpus.empty()) reject(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        output.implementation_ = document::plan::compile(program);
    } else if (profile == "normalized-preparation-draft/1") {
        if (corpus != KUMWE_ENGINE_PREPARATION_CORPUS || corpus.empty()) reject(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        output.implementation_ = preparation::plan::compile(program);
    } else if (profile == "report-materialization-draft/1") {
        if (corpus != KUMWE_ENGINE_REPORT_CORPUS) reject(KUMWE_ENGINE_V1_INCOMPATIBLE_CORPUS);
        output.implementation_ = reporting::report_plan::compile(program);
    } else reject(KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY);
    output.descriptor_ = value(object{{"wire_version", value(std::int64_t{1})}, {"profile", value(profile)},
        {"corpus_digest", value(corpus)}, {"program", std::visit([](const auto& p) { return p.document(); }, output.implementation_)}});
    (void)json::encoded_size(output.descriptor_, max_output);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    if (elapsed >= static_cast<std::int64_t>(max_ms)) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    return output;
}
std::string plan::describe() const { return json::encode(descriptor_, 16777216); }
std::string plan::execute(std::string_view request, const std::atomic<bool>* cancellation) const {
    const auto started = std::chrono::steady_clock::now();
    auto envelope = json::parse(request);
    shape(envelope, {"wire_version", "documents", "limits"});
    version(envelope.at("wire_version"));
    const auto& limits = envelope.at("limits");
    shape(limits, {"max_input_bytes", "max_output_bytes", "max_documents", "max_findings", "max_instructions", "max_milliseconds"});
    const auto max_input = integer(limits.at("max_input_bytes"), 1, 67108864);
    if (request.size() > max_input) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    const auto max_output = integer(limits.at("max_output_bytes"), 1, 67108864);
    const auto max_documents = integer(limits.at("max_documents"), 1, 4096);
    const auto max_findings = integer(limits.at("max_findings"), 1, 65536);
    auto budget = integer(limits.at("max_instructions"), 1, 1000000000);
    const auto max_ms = integer(limits.at("max_milliseconds"), 1, 600000);
    auto checkpoint = [&]() {
        if (cancellation != nullptr && cancellation->load(std::memory_order_relaxed)) reject(KUMWE_ENGINE_V1_CANCELLED);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        if (elapsed >= static_cast<std::int64_t>(max_ms)) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    };
    checkpoint();
    auto& documents = std::get<object>(envelope.data).at("documents");
    if (!documents.is<list>()) reject();
    if (documents.as<list>().size() > max_documents) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    std::string output = "{\"results\":[";
    constexpr std::string_view suffix = "],\"wire_version\":1}";
    bool first_result = true;
    std::set<std::string> seen;
    std::size_t findings_used = 0;
    for (auto& document : std::get<list>(documents.data)) {
        checkpoint();
        const bool opaque = document.find("input") != nullptr;
        if (opaque) shape(document, {"correlation", "input"});
        else shape(document, {"correlation", "fields", "lines"});
        const auto& correlation = text(document.at("correlation"));
        if (!token(correlation) || !seen.emplace(correlation).second) reject();
        // The input tree is already operation-owned. Direct envelopes can lend
        // their lines and move their fields into document execution without
        // duplicating a complete document. Opaque payloads need one parse only.
        value decoded;
        value* admitted = &document;
        if (opaque) {
            decoded = json::parse(text(document.at("input")), max_input);
            shape(decoded, {"fields", "lines"});
            admitted = &decoded;
        }
        auto& fields = std::get<object>(admitted->data).at("fields");
        const auto& lines = admitted->at("lines");
        if (!fields.is<object>() || !(lines.is<object>() || lines.is<std::nullptr_t>())) reject();
        auto result = std::visit([&](const auto& compiled) -> value {
            using T = std::decay_t<decltype(compiled)>;
            if constexpr (std::is_same_v<T, vm::formula>) {
                return value(object{{"value", compiled.evaluate(fields, lines, budget, max_output)}, {"findings", value(list{})}});
            } else if constexpr (std::is_same_v<T, reporting::report_plan>) {
                shape(fields, {"rows"});
                if (!lines.is<object>() || !lines.as<object>().empty()) reject();
                return value(object{{"rows", compiled.materialize(fields.at("rows"), budget, 100000, max_output)},
                    {"findings", value(list{})}});
            } else if constexpr (std::is_same_v<T, kumwe::engine::document::plan>) {
                return compiled.execute_owned(std::move(fields), lines, budget, max_findings - findings_used, max_output);
            } else {
                return compiled.execute(fields, lines, budget, max_findings - findings_used, max_output);
            }
        }, implementation_);
        findings_used += result.at("findings").as<list>().size();
        list portable_findings;
        std::int64_t ordinal = 0;
        for (const auto& finding : result.at("findings").as<list>()) {
            portable_findings.emplace_back(object{{"wire_version", value(std::int64_t{1})},
                {"code", finding.at("code")}, {"severity", value("error")},
                {"path", value(object{{"wire_version", value(std::int64_t{1})},
                    {"segments", value(list{finding.at("field")})}})},
                {"location", value(object{{"wire_version", value(std::int64_t{1})},
                    {"unit", descriptor_.at("profile")}, {"rule", finding.at("code")}, {"ordinal", value(ordinal)}})},
                {"ordinal", value(ordinal)}, {"parameters", value(list{})}});
            ++ordinal;
        }
        // Retain exact canonical payload bytes through the Zend transport; decoding
        // to PHP arrays alone would erase the empty object/list distinction.
        const value encoded_result(json::encode(result, max_output));
        const auto encoded_correlation = json::encode(value(correlation), max_output);
        const auto encoded_findings = json::encode(value(std::move(portable_findings)), max_output);
        const auto quoted_result = json::encode(encoded_result, max_output);
        // Keep both public representations without encoding the result tree twice.
        // The parts are encoded JSON values and keys remain in canonical order.
        const std::string_view parts[] = {"{\"correlation\":", encoded_correlation,
            ",\"findings\":", encoded_findings, ",\"result\":", encoded_result.as<std::string>(),
            ",\"result_json\":", quoted_result, "}"};
        std::size_t item_size = 0;
        for (const auto part : parts) {
            if (part.size() > max_output - item_size) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
            item_size += part.size();
        }
        // Account for exactly the final wire bytes, including separators and
        // suffix. A valid nonempty result fits when its limit equals its size.
        // Refusal remains atomic because output is not published until success.
        const auto remaining = max_output - std::min<std::uint64_t>(max_output, output.size());
        const auto overhead = suffix.size() + (first_result ? 0U : 1U);
        if (output.size() > max_output || overhead > remaining || item_size > remaining - overhead)
            reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        if (!first_result) output.push_back(',');
        for (const auto part : parts) output.append(part);
        first_result = false;
    }
    checkpoint();
    if (output.size() > max_output || suffix.size() > max_output - output.size()) reject(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    output.append(suffix);
    checkpoint();
    return output;
}
}
