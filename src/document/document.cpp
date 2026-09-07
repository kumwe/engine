#include "document.hpp"
#include "computed_value.hpp"
#include "batch.hpp"
#include "vm/error.hpp"
#include "decimal/decimal.hpp"
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
void charge(std::uint64_t& budget) {
    if (budget == 0) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    --budget;
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
        shape(item, {"handle", "required", "nullable", "formula", "validators", "type", "precision", "scale", "length", "normalizers"});
        field next;
        next.handle = handle(member(item, "handle"));
        if (!seen.emplace(next.handle).second) invalid();
        next.required = flag(member(item, "required"), false);
        next.nullable = flag(member(item, "nullable"), true);
        if (next.required && next.nullable) invalid();
        const auto& formula = member(item, "formula");
        if (!formula.is<std::nullptr_t>()) {
            next.computation.push_back(vm::formula::compile(formula));
            const auto& dependencies = next.computation.front().dependencies();
            result.projected_fields_.insert(dependencies.begin(), dependencies.end());
        }
        auto validators = member(item, "validators");
        if (validators.is<std::nullptr_t>()) validators = value(list{});
        if (!validators.is<list>() || validators.as<list>().size() > 32) invalid();
        for (const auto& rule : validators.as<list>()) {
            if (!rule.is<object>()) invalid();
            next.validators.emplace_back(rule);
        }
        object descriptor{{"handle", value(next.handle)}, {"required", value(next.required)},
            {"nullable", value(next.nullable)}, {"validators", validators},
            {"formula", next.computation.empty() ? value() : next.computation.front().document()}};
        for (const auto key : {"type", "precision", "scale", "length", "normalizers"}) if (const auto* metadata = item.find(key)) descriptor.emplace(key, *metadata);
        next.definition = value(descriptor);
        if (!next.computation.empty()) validate_computed_definition(next.definition);
        canonical_fields.emplace_back(std::move(descriptor));
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
        const auto& dependencies = condition.dependencies();
        result.projected_fields_.insert(dependencies.begin(), dependencies.end());
        canonical_invariants.emplace_back(object{{"handle", value(name)}, {"condition", condition.document()}});
        result.invariants_.push_back(invariant{name, std::move(condition)});
    }
    result.document_ = value(object{{"fields", value(std::move(canonical_fields))},
        {"invariants", value(std::move(canonical_invariants))}, {"require_all", value(result.require_all_)}});
    return result;
}
value plan::execute(const value& fields, const value& lines, std::uint64_t& budget,
                    std::size_t finding_limit, std::size_t output_limit, const execution_context* context) const {
    return execute_impl(fields, lines, budget, finding_limit, output_limit, context, nullptr);
}
value plan::execute_owned(value&& fields, const value& lines, std::uint64_t& budget,
                    std::size_t finding_limit, std::size_t output_limit, const execution_context* context) const {
    return execute_impl(fields, lines, budget, finding_limit, output_limit, context, &fields);
}
value plan::execute_impl(const value& fields, const value& lines, std::uint64_t& budget,
                    std::size_t finding_limit, std::size_t output_limit, const execution_context* context,
                    value* owned_fields) const {
    if (!fields.is<object>() || !(lines.is<object>() || lines.is<std::nullptr_t>()))
        throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    std::vector<const value*> instance_values;
    for (const auto& [key, item] : fields.as<object>()) { (void)key; instance_values.push_back(&item); }
    if (lines.is<object>()) for (const auto& [key, collection] : lines.as<object>()) {
        (void)key;
        if (!collection.is<list>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        for (const auto& row : collection.as<list>()) {
            if (!row.is<object>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            for (const auto& [name, item] : row.as<object>()) { (void)name; instance_values.push_back(&item); }
        }
    }
    validate_normalized_instances(instance_values);
    value values(object{});
    value projected_values(object{});
    std::set<std::string> invalid_fields;
    list findings;
    constexpr std::size_t envelope_bytes = std::string_view("{\"values\":,\"findings\":}").size();
    std::size_t retained_values = 2, retained_findings = 2;
    auto fits = [&](std::size_t projected_values, std::size_t projected_findings) {
        if (output_limit < envelope_bytes || projected_values > output_limit - envelope_bytes
            || projected_findings > output_limit - envelope_bytes - projected_values)
            throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    };
    auto store_value = [&](const std::string& key, const value& candidate, bool already_owned = false) {
        const auto encoded_size = json::encoded_size(output_value(candidate), output_limit);
        auto projected = retained_values;
        if (const auto* previous = values.find(key)) projected -= json::encoded_size(output_value(*previous), output_limit);
        else projected += json::encoded_size(value(key), output_limit) + 1 + (retained_values == 2 ? 0 : 1);
        projected += encoded_size;
        fits(projected, retained_findings);
        const bool needs_projection = projected_fields_.contains(key);
        value expression;
        if (needs_projection) expression = expression_value(candidate);
        if (!already_owned) std::get<object>(values.data).insert_or_assign(key, candidate);
        if (needs_projection)
            std::get<object>(projected_values.data).insert_or_assign(key, std::move(expression));
        retained_values = projected;
    };
    for (const auto& [key, item] : fields.as<object>()) {
        store_value(key, item, owned_fields != nullptr);
    }
    // Admission and each retained-output check ran before this transfer. Borrowed
    // callers still receive the incremental-copy behavior above.
    if (owned_fields != nullptr) values = std::move(*owned_fields);
    value normalized_lines = lines;
    if (lines.is<object>()) for (auto& [key, collection] : std::get<object>(normalized_lines.data)) {
        (void)key;
        if (!collection.is<list>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        for (auto& row : std::get<list>(collection.data)) {
            if (!row.is<object>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            for (auto& [name, item] : std::get<object>(row.data)) {
                (void)name;
                const auto flattened = expression_value(item);
                item = flattened;
            }
        }
    }
    finding_sink finding = [&](const std::string& field, const std::string& code) {
        if (findings.size() >= finding_limit) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        value next(object{{"field", value(field)}, {"code", value(code)}});
        const auto projected = retained_findings + json::encoded_size(next, output_limit) + (findings.empty() ? 0 : 1);
        fits(retained_values, projected);
        findings.emplace_back(std::move(next));
        retained_findings = projected;
    };
    if (context != nullptr) for (const auto& initial : context->initial_findings) {
        if (!initial.is<object>() || initial.as<object>().size() != 2 || !member(initial, "field").is<std::string>()
            || !member(initial, "code").is<std::string>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
        const auto& name = initial.at("field").as<std::string>(); const auto& code = initial.at("code").as<std::string>();
        finding(name, code);
        if (code == "invalid_type") invalid_fields.emplace(name);
    }
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
                auto output = expression.evaluate(projected_values, value(object{}), budget, output_limit);
                auto normalized = normalize_computed_value(std::move(output), field.definition, budget, output_limit);
                store_value(field.handle, normalized);
                advanced = true;
            } catch (const vm::error&) { finding(field.handle, "formula_failed"); }
            cursor = pending.erase(cursor);
        }
        if (!advanced) break;
    }
    if (require_all_) for (const auto* field : pending) finding(field->handle, "formula_dependency");
    if (context != nullptr && context->after_computation) context->after_computation(values, budget, finding);
    for (const auto& field : fields_) {
        charge(budget);
        if (invalid_fields.contains(field.handle)) continue;
        const auto& candidate = member(values, field.handle);
        if (candidate.is<std::nullptr_t>()) {
            if (field.required) finding(field.handle, "required");
            else if (!field.nullable) finding(field.handle, "not_nullable");
            continue;
        }
        for (const auto& rule : field.validators) {
            charge(budget);
            try {
                if (!rule.judge(output_value(candidate), normalized_exact_decimal(candidate), normalized_domain_object(candidate), field.definition)) finding(field.handle, rule.name());
            } catch (const vm::error&) { finding(field.handle, "validator_invalid"); }
        }
    }
    for (const auto& invariant : invariants_) {
        if (lines.is<std::nullptr_t>() && !invariant.condition.line_dependencies().empty()) continue;
        try {
            const auto satisfied = invariant.condition.evaluate(projected_values, normalized_lines, budget, output_limit);
            if (satisfied != value(true)) finding(invariant.handle, "invariant." + invariant.handle);
        } catch (const vm::error&) { finding(invariant.handle, "invariant_invalid"); }
    }
    // Domain values remain intact through validation. Canonicalize the owned map
    // in place only after every consumer of domain type and identity has finished.
    for (auto& [key, item] : std::get<object>(values.data)) { (void)key; item = output_value(item); }
    object result_fields;
    result_fields.emplace("values", std::move(values));
    result_fields.emplace("findings", value(std::move(findings)));
    value result(std::move(result_fields));
    (void)json::encoded_size(result, output_limit);
    return result;
}
}
