#include "document.hpp"
#include "batch.hpp"
#include "vm/error.hpp"
#include "value/php_numeric.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <charconv>
#include <set>

namespace kumwe::engine::document {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
const value null;
const value& member(const value& v, std::string_view k) { const auto* p = v.find(k); return p ? *p : null; }
[[noreturn]] void invalid() { throw refusal(KUMWE_ENGINE_V1_INVALID_PROGRAM); }
void shape(const value& v, std::initializer_list<std::string_view> allowed) {
    if (!v.is<object>()) invalid();
    for (const auto& [key, unused] : v.as<object>()) {
        (void)unused;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) invalid();
    }
}
std::string handle(const value& v) {
    if (!v.is<std::string>()) invalid();
    const auto& s = v.as<std::string>();
    if (s.empty() || s.size() > 63 || s[0] < 'a' || s[0] > 'z') invalid();
    for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) invalid();
    return s;
}
bool flag(const value& v, bool fallback) {
    if (v.is<std::nullptr_t>()) return fallback;
    if (!v.is<bool>()) invalid();
    return v.as<bool>();
}
bool scalar(const value& v) {
    return v.is<std::nullptr_t>() || v.is<bool>() || v.is<std::int64_t>() || v.is<std::string>();
}
void charge(std::uint64_t& budget) {
    if (budget == 0) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    --budget;
}
std::size_t characters(const std::string& s) {
    return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), [](char c) {
        return (static_cast<unsigned char>(c) & 0xc0U) != 0x80U;
    }));
}
bool valid_rule(const value& rule, const value& candidate) {
    const auto& name = rule.at("rule").as<std::string>();
    if (name == "integer") return candidate.is<std::int64_t>();
    const auto& argument = member(rule, "value");
    if (name == "min" || name == "max") {
        int order = 0;
        if (candidate.is<std::int64_t>() && argument.is<std::string>()) {
            const auto& digits = argument.as<std::string>();
            std::int64_t limit = 0;
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), limit);
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()
                || digits.empty() || (digits.size() > 1 && digits[0] == '0')
                || (digits.size() > 2 && digits[0] == '-' && digits[1] == '0')) vm::reject("Invalid range bound.");
            order = candidate.as<std::int64_t>() == limit ? 0 : candidate.as<std::int64_t>() < limit ? -1 : 1;
        } else if (candidate.is<std::string>() && argument.is<std::string>()) {
            order = value_compat::php_string_compare(candidate.as<std::string>(), argument.as<std::string>());
        } else vm::reject("Invalid range bound.");
        return name == "min" ? order >= 0 : order <= 0;
    }
    if (name == "one_of") {
        const auto& entries = argument.as<list>();
        return std::find(entries.begin(), entries.end(), candidate) != entries.end();
    }
    if (name != "min_length" && name != "max_length") vm::reject("Unregistered validator.");
    if (!candidate.is<std::string>()) return false;
    const auto length = characters(candidate.as<std::string>());
    const auto limit = static_cast<std::size_t>(argument.as<std::int64_t>());
    return name == "min_length" ? length >= limit : length <= limit;
}
}
plan plan::compile(const value& program) {
    shape(program, {"fields", "invariants", "require_all"});
    const auto& fields = member(program, "fields");
    const auto& invariants = member(program, "invariants");
    if (!fields.is<list>() || fields.as<list>().size() > 512
        || !invariants.is<list>() || invariants.as<list>().size() > 512) invalid();
    plan result;
    result.require_all_ = flag(member(program, "require_all"), true);
    std::set<std::string> seen;
    list canonical_fields;
    for (const auto& item : fields.as<list>()) {
        shape(item, {"handle", "required", "nullable", "formula", "validators"});
        field next;
        next.handle = handle(member(item, "handle"));
        if (!seen.emplace(next.handle).second) invalid();
        next.required = flag(member(item, "required"), false);
        next.nullable = flag(member(item, "nullable"), true);
        if (next.required && next.nullable) invalid();
        const auto& formula = member(item, "formula");
        if (!formula.is<std::nullptr_t>()) next.computation.push_back(vm::formula::compile(formula));
        next.validators = member(item, "validators");
        if (next.validators.is<std::nullptr_t>()) next.validators = value(list{});
        if (!next.validators.is<list>() || next.validators.as<list>().size() > 32) invalid();
        for (const auto& rule : next.validators.as<list>()) {
            shape(rule, {"rule", "value"});
            const auto& name = member(rule, "rule");
            if (!name.is<std::string>()) invalid();
            const auto& token = name.as<std::string>();
            const auto& argument = member(rule, "value");
            if (token == "integer") {
                if (!argument.is<std::nullptr_t>()) invalid();
            } else if (token == "min_length" || token == "max_length") {
                if (!argument.is<std::int64_t>() || argument.as<std::int64_t>() < 0
                    || argument.as<std::int64_t>() > 1000000) invalid();
            } else if (token == "one_of") {
                if (!argument.is<list>() || argument.as<list>().empty() || argument.as<list>().size() > 256) invalid();
                for (const auto& option : argument.as<list>()) if (!scalar(option)) invalid();
            } else if (token == "min" || token == "max") {
                if (!scalar(argument)) invalid();
            } else if (token == "pattern" || token == "email" || token == "url" || token == "uuid" || token == "decimal") {
                // Regex, codec-aware ranges and host value kinds require their exact profiles.
                throw refusal(KUMWE_ENGINE_V1_INCOMPATIBLE_CAPABILITY);
            }
        }
        canonical_fields.emplace_back(object{{"handle", value(next.handle)}, {"required", value(next.required)},
            {"nullable", value(next.nullable)}, {"validators", next.validators},
            {"formula", next.computation.empty() ? value() : next.computation.front().document()}});
        result.fields_.push_back(std::move(next));
    }
    seen.clear();
    list canonical_invariants;
    for (const auto& item : invariants.as<list>()) {
        shape(item, {"handle", "condition"});
        const auto name = handle(member(item, "handle"));
        if (!seen.emplace(name).second) invalid();
        auto condition = vm::formula::compile(member(item, "condition"));
        if (condition.document().at("type") != value("boolean")) invalid();
        canonical_invariants.emplace_back(object{{"handle", value(name)}, {"condition", condition.document()}});
        result.invariants_.push_back(invariant{name, std::move(condition)});
    }
    result.document_ = value(object{{"fields", value(std::move(canonical_fields))},
        {"invariants", value(std::move(canonical_invariants))}, {"require_all", value(result.require_all_)}});
    return result;
}
value plan::execute(const value& fields, const value& lines, std::uint64_t& budget,
                    std::size_t finding_limit, std::size_t output_limit) const {
    if (!fields.is<object>() || !(lines.is<object>() || lines.is<std::nullptr_t>()))
        throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    for (const auto& [key, item] : fields.as<object>()) {
        (void)key;
        if (!scalar(item)) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    }
    if (lines.is<object>()) for (const auto& [key, collection] : lines.as<object>()) {
        (void)key;
        if (!collection.is<list>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        for (const auto& row : collection.as<list>()) {
            if (!row.is<object>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            for (const auto& [name, item] : row.as<object>()) {
                (void)name;
                if (!scalar(item)) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            }
        }
    }
    value values = fields;
    list findings;
    auto finding = [&](const std::string& field, const std::string& code) {
        if (findings.size() >= finding_limit) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        findings.emplace_back(object{{"field", value(field)}, {"code", value(code)}});
    };
    std::vector<const field*> pending;
    for (const auto& field : fields_) if (!field.computation.empty()) pending.push_back(&field);
    // Preserve the source pass bound, discovery order and missing-dependency behavior.
    for (std::size_t pass = 0; !pending.empty() && pass <= pending.size(); ++pass) {
        bool advanced = false;
        for (auto cursor = pending.begin(); cursor != pending.end();) {
            charge(budget);
            const auto& field = **cursor;
            const auto& expression = field.computation.front();
            const auto& dependencies = expression.dependencies();
            if (std::any_of(dependencies.begin(), dependencies.end(), [&](const auto& key) { return !values.find(key); })) {
                ++cursor;
                continue;
            }
            try {
                // The source computed-field pass does not supply owned lines to expressions.
                auto output = expression.evaluate(values, value(object{}), budget, output_limit);
                std::get<object>(values.data).insert_or_assign(field.handle, std::move(output));
                advanced = true;
            } catch (const vm::error&) { finding(field.handle, "formula_failed"); }
            cursor = pending.erase(cursor);
        }
        if (!advanced) break;
    }
    if (require_all_) for (const auto* field : pending) finding(field->handle, "formula_dependency");
    for (const auto& field : fields_) {
        charge(budget);
        const auto& candidate = member(values, field.handle);
        if (candidate.is<std::nullptr_t>()) {
            if (field.required) finding(field.handle, "required");
            else if (!field.nullable) finding(field.handle, "not_nullable");
            continue;
        }
        for (const auto& rule : field.validators.as<list>()) {
            charge(budget);
            try {
                if (!valid_rule(rule, candidate)) finding(field.handle, rule.at("rule").as<std::string>());
            } catch (const vm::error&) { finding(field.handle, "validator_invalid"); }
        }
    }
    for (const auto& invariant : invariants_) {
        if (lines.is<std::nullptr_t>() && !invariant.condition.line_dependencies().empty()) continue;
        try {
            const auto satisfied = invariant.condition.evaluate(values, lines, budget, output_limit);
            if (satisfied != value(true)) finding(invariant.handle, "invariant." + invariant.handle);
        } catch (const vm::error&) { finding(invariant.handle, "invariant_invalid"); }
    }
    value result(object{{"values", std::move(values)}, {"findings", value(std::move(findings))}});
    (void)json::encode(result, output_limit);
    return result;
}
}
