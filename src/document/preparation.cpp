#include "preparation.hpp"
#include "batch.hpp"
#include "vm/error.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <map>
#include <set>

namespace kumwe::engine::preparation {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
const value null;
[[noreturn]] void invalid(std::uint32_t code) { throw refusal(code); }
void shape(const value& input, std::initializer_list<std::string_view> keys, std::uint32_t code) {
    if (!input.is<object>() || input.as<object>().size() != keys.size()) invalid(code);
    for (const auto& [key, unused] : input.as<object>()) {
        (void)unused;
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) invalid(code);
    }
}
void charge(std::uint64_t& budget) {
    if (budget == 0) invalid(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    --budget;
}
const value& member(const value& input, std::string_view name) {
    const auto* result = input.find(name);
    return result == nullptr ? null : *result;
}
bool flag(const value& input, std::uint32_t code) {
    if (!input.is<bool>()) invalid(code);
    return input.as<bool>();
}
std::string handle(const value& input, std::uint32_t code, bool declared) {
    if (!input.is<std::string>()) invalid(code);
    const auto& name = input.as<std::string>();
    if (name.empty() || name.size() > 191 || !json::valid_utf8(name)) invalid(code);
    if (declared) {
        if (name.size() > 63 || name.front() < 'a' || name.front() > 'z') invalid(code);
        for (char c : name) if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_') invalid(code);
    }
    return name;
}
value expression_values(const value& input) {
    object output;
    for (const auto& [key, item] : input.as<object>()) {
        output.emplace(key, document::expression_value(item));
    }
    return value(std::move(output));
}
struct admission final { value item; bool valid; };
admission admitted(const value& input, std::uint32_t code) {
    shape(input, {"value", "valid"}, code);
    const bool valid = flag(input.at("valid"), code);
    const auto& item = input.at("value");
    if ((valid && !document::normalized_value_valid(item)) || (!valid && !item.is<std::nullptr_t>())) invalid(code);
    return admission{item, valid};
}
bool has_instances(const value& item) {
    if (document::normalized_domain_object(item) && item.find("instance")) return true;
    if (item.is<list>()) for (const auto& child : item.as<list>()) if (has_instances(child)) return true;
    if (item.is<object>()) for (const auto& [key, child] : item.as<object>()) {
        (void)key;
        if (has_instances(child)) return true;
    }
    return false;
}
void conditions(const field& field, const value& values, std::uint64_t& budget,
                std::size_t output_limit, const document::finding_sink& finding) {
    const auto judge = [&](const std::vector<vm::formula>& condition, const std::string& failure) {
        if (condition.empty()) return true;
        charge(budget);
        try {
            if (condition.front().evaluate(values, value(object{}), budget, output_limit) == value(true)) return true;
            finding(field.handle, failure);
        } catch (const vm::error&) { finding(field.handle, "condition_failed"); }
        return false;
    };
    if (judge(field.visibility, "not_visible")) (void)judge(field.editability, "not_editable");
}
}
plan plan::compile(const value& program) {
    constexpr auto code = KUMWE_ENGINE_V1_INVALID_PROGRAM;
    shape(program, {"fields", "validation"}, code);
    const auto& fields = program.at("fields");
    if (!fields.is<list>() || fields.as<list>().size() > 512) invalid(code);
    plan output;
    output.validation_ = document::plan::compile(program.at("validation"));
    const auto& validation_fields = output.validation_.document().at("fields").as<list>();
    if (validation_fields.size() != fields.as<list>().size()) invalid(code);
    std::set<std::string> seen;
    list canonical;
    for (const auto& declaration : fields.as<list>()) {
        shape(declaration, {"handle", "identity", "sequence", "computed", "server_only", "read_only",
            "immutable_after_create", "default", "visibility_condition", "editability_condition"}, code);
        field next;
        next.handle = handle(declaration.at("handle"), code, true);
        if (!seen.emplace(next.handle).second) invalid(code);
        const auto& validated = validation_fields.at(canonical.size());
        if (validated.at("handle") != value(next.handle)) invalid(code);
        next.identity = flag(declaration.at("identity"), code);
        next.sequence = flag(declaration.at("sequence"), code);
        next.computed = flag(declaration.at("computed"), code);
        next.server_only = flag(declaration.at("server_only"), code);
        next.read_only = flag(declaration.at("read_only"), code);
        next.immutable = flag(declaration.at("immutable_after_create"), code);
        if (next.identity && next.sequence) invalid(code);
        if (!validated.at("formula").is<std::nullptr_t>() && !next.computed) invalid(code);
        if (const auto* type = validated.find("type")) {
            const bool identity_type = *type == value("core.uuid") || *type == value("core.reference_identity");
            if (next.identity != identity_type || next.sequence != (*type == value("core.sequence"))) invalid(code);
        }
        const auto default_result = admitted(declaration.at("default"), code);
        if (has_instances(default_result.item)) invalid(code);
        next.default_value = default_result.item;
        next.default_valid = default_result.valid;
        auto canonical_field = declaration.as<object>();
        for (const auto key : {"visibility_condition", "editability_condition"}) {
            const auto& expression = declaration.at(key);
            if (expression.is<std::nullptr_t>()) continue;
            auto compiled = vm::formula::compile(expression);
            if (compiled.document().at("type") != value("boolean") || !compiled.line_dependencies().empty()) invalid(code);
            canonical_field.insert_or_assign(key, compiled.document());
            if (std::string_view(key) == "visibility_condition") next.visibility.push_back(std::move(compiled));
            else next.editability.push_back(std::move(compiled));
        }
        canonical.emplace_back(std::move(canonical_field));
        output.fields_.push_back(std::move(next));
    }
    output.document_ = value(object{{"fields", value(std::move(canonical))},
        {"validation", output.validation_.document()}});
    return output;
}
value plan::execute(const value& request, const value& lines, std::uint64_t& budget,
                    std::size_t finding_limit, std::size_t output_limit) const {
    constexpr auto code = KUMWE_ENGINE_V1_INVALID_INPUT;
    shape(request, {"operation", "current", "input", "identity", "allocated"}, code);
    const auto& operation = request.at("operation");
    if (operation != value("create") && operation != value("update")) invalid(code);
    const bool create = operation == value("create");
    const auto& current = request.at("current");
    const auto& input = request.at("input");
    const auto& identity = request.at("identity");
    const auto& allocated = request.at("allocated");
    if (!current.is<object>() || !input.is<list>() || !allocated.is<object>()
        || input.as<list>().size() > 512 || current.as<object>().size() > 512
        || allocated.as<object>().size() > 512 || !identity.is<std::string>()
        || identity.as<std::string>().empty() || identity.as<std::string>().size() > 191) invalid(code);
    if (create && !current.as<object>().empty()) invalid(code);
    std::vector<const value*> instance_inputs;
    for (const auto& [key, item] : current.as<object>()) {
        (void)handle(value(key), code, false);
        if (!document::normalized_value_valid(item)) invalid(code);
        instance_inputs.push_back(&item);
    }
    std::map<std::string, const value*, std::less<>> submitted;
    for (const auto& entry : input.as<list>()) {
        charge(budget);
        shape(entry, {"handle", "submitted", "normalized"}, code);
        const auto name = handle(entry.at("handle"), code, false);
        if (!submitted.emplace(name, &entry).second) invalid(code);
        (void)admitted(entry.at("normalized"), code);
        instance_inputs.push_back(&entry.at("submitted"));
        instance_inputs.push_back(&entry.at("normalized").at("value"));
    }
    for (const auto& [key, item] : allocated.as<object>()) {
        (void)handle(value(key), code, false);
        (void)admitted(item, code);
        instance_inputs.push_back(&item.at("value"));
    }
    for (const auto& field : fields_) instance_inputs.push_back(&field.default_value);
    if (!(lines.is<object>() || lines.is<std::nullptr_t>())) invalid(code);
    if (lines.is<object>()) for (const auto& [key, collection] : lines.as<object>()) {
        (void)key;
        if (!collection.is<list>()) invalid(code);
        for (const auto& row : collection.as<list>()) {
            if (!row.is<object>()) invalid(code);
            for (const auto& [name, item] : row.as<object>()) { (void)name; instance_inputs.push_back(&item); }
        }
    }
    document::validate_normalized_instances(instance_inputs);
    document::execution_context context;
    value values(object{});
    constexpr std::size_t envelope_bytes = std::string_view("{\"values\":,\"findings\":}").size();
    std::size_t retained_values = 2, retained_findings = 2;
    const auto fits = [&](std::size_t projected_values, std::size_t projected_findings) {
        if (output_limit < envelope_bytes || projected_values > output_limit - envelope_bytes
            || projected_findings > output_limit - envelope_bytes - projected_values)
            invalid(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    };
    const auto value_bytes = [&](const value& item) {
        return json::encode(document::output_value(item), output_limit).size();
    };
    const auto store_value = [&](const std::string& name, const value& item) {
        auto projected = retained_values;
        if (const auto* previous = values.find(name)) projected -= value_bytes(*previous);
        else projected += json::encode(value(name), output_limit).size() + 1 + (values.as<object>().empty() ? 0 : 1);
        projected += value_bytes(item);
        fits(projected, retained_findings);
        std::get<object>(values.data).insert_or_assign(name, item);
        retained_values = projected;
    };
    fits(retained_values, retained_findings);
    if (!create) for (const auto& [name, item] : current.as<object>()) store_value(name, item);
    const document::finding_sink finding = [&](const std::string& field, const std::string& reason) {
        if (context.initial_findings.size() >= finding_limit) invalid(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        value next(object{{"field", value(field)}, {"code", value(reason)}});
        const auto projected = retained_findings + json::encode(next, output_limit).size()
            + (context.initial_findings.empty() ? 0 : 1);
        fits(retained_values, projected);
        context.initial_findings.emplace_back(std::move(next));
        retained_findings = projected;
    };
    std::map<std::string, const field*, std::less<>> declarations;
    for (const auto& field : fields_) declarations.emplace(field.handle, &field);
    auto put = [&](const std::string& name, const admission& result) {
        if (!result.valid) finding(name, "invalid_type");
        else store_value(name, result.item);
    };
    std::vector<const field*> conditioned;
    if (create) {
        for (const auto& entry : input.as<list>()) {
            const auto name = entry.at("handle").as<std::string>();
            if (!declarations.contains(name)) finding(name, "unknown");
        }
        for (const auto& field : fields_) {
            charge(budget);
            const auto found = submitted.find(field.handle);
            const bool present = found != submitted.end();
            if (field.identity) {
                store_value(field.handle, identity);
            } else if (field.sequence) {
                if (present) finding(field.handle, "read_only");
                else if (!allocated.find(field.handle)) finding(field.handle, "allocation");
                else {
                    const auto result = admitted(allocated.at(field.handle), code);
                    if (result.valid && result.item.is<std::nullptr_t>()) finding(field.handle, "allocation");
                    else put(field.handle, result);
                }
            } else if (field.computed || ((field.server_only || field.read_only) && present)) {
                if (present) finding(field.handle, "read_only");
            } else {
                if (present && (!field.visibility.empty() || !field.editability.empty())) conditioned.push_back(&field);
                put(field.handle, present ? admitted(found->second->at("normalized"), code)
                    : admission{field.default_value, field.default_valid});
            }
        }
        context.after_computation = [&](const value& computed, std::uint64_t& remaining,
                                         const document::finding_sink& sink) {
            const auto expression_input = expression_values(computed);
            for (const auto* field : conditioned) conditions(*field, expression_input, remaining, output_limit, sink);
        };
    } else {
        const auto expression_input = expression_values(current);
        for (const auto& entry : input.as<list>()) {
            charge(budget);
            const auto name = entry.at("handle").as<std::string>();
            const auto found = declarations.find(name);
            if (found == declarations.end()) { finding(name, "unknown"); continue; }
            const auto& field = *found->second;
            if (field.identity || field.immutable) {
                const auto& old = member(current, name);
                if (!document::normalized_strict_equal(old, entry.at("submitted"))) finding(name, "immutable");
                continue;
            }
            if (field.server_only || field.read_only || field.computed) { finding(name, "read_only"); continue; }
            const auto previous = context.initial_findings.size();
            conditions(field, expression_input, budget, output_limit, finding);
            if (context.initial_findings.size() != previous) continue;
            put(name, admitted(entry.at("normalized"), code));
        }
    }
    return validation_.execute(values, lines, budget, finding_limit, output_limit, &context);
}
}
