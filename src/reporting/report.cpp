#include "report.hpp"
#include "converted.hpp"
#include "value/php_numeric.hpp"
#include "vm/decimal.hpp"
#include "vm/error.hpp"
#include "batch.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <limits>
#include <set>
#include <string_view>

namespace kumwe::engine::reporting {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
[[noreturn]] void invalid() { throw vm::error(true, "A report computation plan is invalid."); }
[[noreturn]] void unavailable() { throw vm::error(false, "The report is unavailable."); }
void charge(std::uint64_t& budget, std::uint64_t cost = 1) {
    if (cost > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= cost;
}
bool one_of(std::string_view input, std::initializer_list<std::string_view> allowed) {
    return std::find(allowed.begin(), allowed.end(), input) != allowed.end();
}
void keys(const value& input, std::initializer_list<std::string_view> allowed) {
    if (!input.is<object>()) invalid();
    for (const auto& [key, child] : input.as<object>()) {
        (void)child;
        if (!one_of(key, allowed)) invalid();
    }
}
std::string text(const value& input, std::string_view key) {
    const auto* child = input.find(key);
    if (!child || !child->is<std::string>()) invalid();
    return child->as<std::string>();
}
bool handle(std::string_view input) {
    if (input.empty() || input.size() > 63 || input.front() < 'a' || input.front() > 'z') return false;
    for (const char c : input)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
std::string alias(const value& input, std::string_view key) {
    auto result = text(input, key);
    if (!handle(result)) invalid();
    return result;
}
const list& collection(const value& input, std::string_view key, std::size_t maximum) {
    static const list empty;
    const auto* child = input.find(key);
    if (!child) return empty;
    if (!child->is<list>() || child->as<list>().size() > maximum) invalid();
    return child->as<list>();
}
bool scalar(const value& input) {
    return input.is<std::nullptr_t>() || input.is<bool>() || input.is<std::int64_t>() || input.is<std::string>();
}
std::string scalar_text(const value& input) {
    if (input.is<std::string>()) return input.as<std::string>();
    if (input.is<std::int64_t>()) return std::to_string(input.as<std::int64_t>());
    if (input.is<bool>()) return input.as<bool>() ? "1" : "";
    unavailable();
}
bool numeric(const value& input) {
    return input.is<std::int64_t>() || (input.is<std::string>() && vm::decimal_literal(input.as<std::string>()));
}
int compare(const value& left, const value& right, std::uint64_t& budget) {
    const auto width = [](const value& cell) -> std::uint64_t { return cell.is<std::string>() ? cell.as<std::string>().size() : 20; };
    charge(budget,width(left) + width(right));
    const auto l = scalar_text(left), r = scalar_text(right);
    if (numeric(left) && numeric(right)) return vm::decimal::parse(l).compare(vm::decimal::parse(r));
    return value_compat::php_string_compare(l,r);
}
std::size_t characters(std::string_view input) {
    std::size_t count = 0;
    for (const char c : input) if ((static_cast<unsigned char>(c) & 0xc0U) != 0x80U) ++count;
    return count;
}
bool date(std::string_view input) {
    if (input.size() != 10) return false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (i == 4 || i == 7) { if (input[i] != '-') return false; }
        else if (input[i] < '0' || input[i] > '9') return false;
    }
    return true;
}
bool typed(std::string_view type, const value& input, std::uint64_t& budget) {
    if (input.is<std::nullptr_t>()) return true;
    if (type == "boolean") return input.is<bool>();
    if (type == "integer") return input.is<std::int64_t>();
    if (type == "decimal") return numeric(input);
    if (!input.is<std::string>()) return false;
    const auto& s = input.as<std::string>();
    if (type == "converted_money" || type == "converted_quantity") return converted_literal(s, type == "converted_money", budget);
    if (type == "string") return characters(s) <= 4096;
    if (type == "date") return date(s);
    if (type == "date_time") {
        if (s.size() < 12 || !date(std::string_view(s).substr(0,10)) || s[10] != 'T' || characters(std::string_view(s).substr(11)) > 64) return false;
        for (std::size_t i = 11; i < s.size(); ++i) if (static_cast<unsigned char>(s[i]) < 32) return false;
        return true;
    }
    if (type == "identifier") {
        if (!s.empty() && s.size() <= 191 && s.front() >= 'a' && s.front() <= 'z') {
            bool accepted = true;
            for (const char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) accepted = false;
            if (accepted) return true;
        }
        if (s.size() != 36 || s[14] < '1' || s[14] > '8' || !one_of(std::string_view(s).substr(19,1), {"8","9","a","b"})) return false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
            else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
        }
        return true;
    }
    return false;
}
}

report_plan report_plan::compile(const json::value& computation) {
    keys(computation, {"columns","groups","aggregates","formulas","sorts"});
    const auto& columns = collection(computation, "columns", 64);
    const auto& groups = collection(computation, "groups", 4);
    const auto& aggregates = collection(computation, "aggregates", 16);
    const auto& formulas = collection(computation, "formulas", 16);
    const auto& sorts = collection(computation, "sorts", 5);
    if (columns.empty()) invalid();
    report_plan result;
    std::map<std::string,std::string> column_types;
    for (const auto& column : columns) {
        keys(column, {"alias","type","label","source"});
        const auto name = alias(column, "alias"), type = text(column, "type");
        if (!one_of(type, {"boolean","integer","decimal","string","identifier","date","date_time","converted_money","converted_quantity"}) || !column_types.emplace(name,type).second) invalid();
        for (const auto key : {"label","source"}) {
            const auto* metadata = column.find(key);
            if (metadata && !metadata->is<std::string>()) invalid();
        }
    }
    std::set<std::string> outputs;
    for (const auto& group : groups) {
        keys(group, {"column"});
        auto name = alias(group, "column");
        if (!column_types.contains(name)) invalid();
        outputs.emplace(name); result.groups_.push_back(std::move(name));
    }
    if (groups.empty() && aggregates.empty()) for (const auto& [name,type] : column_types) { (void)type; outputs.emplace(name); }
    for (const auto& aggregate : aggregates) {
        keys(aggregate, {"alias","function","column"});
        const auto name = alias(aggregate, "alias"), function = text(aggregate, "function");
        if (!one_of(function, {"count","sum","avg","min","max"}) || !outputs.emplace(name).second) invalid();
        const auto* source = aggregate.find("column");
        std::string column;
        if (function == "count") { if (source && !source->is<std::nullptr_t>()) invalid(); }
        else {
            column = alias(aggregate, "column");
            if (!column_types.contains(column)) invalid();
            const auto& type = column_types.at(column);
            if ((one_of(function, {"sum","avg"}) && !one_of(type, {"integer","decimal"})) || (one_of(function, {"min","max"}) && type == "boolean")) invalid();
        }
        result.aggregates_.push_back({name,function,std::move(column)});
    }
    for (const auto& formula : formulas) {
        keys(formula, {"alias","type","expression","label"});
        const auto name = alias(formula, "alias"), type = text(formula, "type");
        if (!one_of(type, {"boolean","integer","decimal","string","identifier","date","date_time","converted_money","converted_quantity"})) invalid();
        const auto* expression = formula.find("expression");
        if (!expression) invalid();
        auto compiled = vm::formula::compile(*expression);
        if (!compiled.line_dependencies().empty()) invalid();
        for (const auto& dependency : compiled.dependencies()) if (!outputs.contains(dependency)) invalid();
        if (!outputs.emplace(name).second) invalid();
        result.formulas_.push_back({name,type,std::move(compiled)});
    }
    for (const auto& sort : sorts) {
        keys(sort, {"output","direction","nulls_last"});
        auto output = alias(sort, "output");
        if (!outputs.contains(output)) invalid();
        const auto* direction = sort.find("direction");
        const auto* nulls = sort.find("nulls_last");
        const auto order = direction ? text(sort, "direction") : "asc";
        if (!one_of(order, {"asc","desc"}) || (nulls && !nulls->is<bool>())) invalid();
        result.sorts_.push_back({std::move(output),order == "desc",nulls ? nulls->as<bool>() : true});
    }
    result.document_ = computation;
    return result;
}

json::value report_plan::materialize(const json::value& authorized_rows, std::uint64_t& budget,
                                    std::size_t max_rows, std::size_t max_output_bytes) const {
    if (!authorized_rows.is<list>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
    const auto& input = authorized_rows.as<list>();
    if (max_rows > 100000 || input.size() > max_rows || max_output_bytes > 16777216) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    charge(budget);
    for (const auto& row : input) {
        charge(budget);
        if (!row.is<object>()) unavailable();
        for (const auto& [key, cell] : row.as<object>()) {
            charge(budget, static_cast<std::uint64_t>(key.size()) + 1);
            if (!scalar(cell)) unavailable();
            if (cell.is<std::string>()) charge(budget, cell.as<std::string>().size());
        }
    }
    list rows;
    std::size_t output_bytes = 2;
    if (max_output_bytes < output_bytes) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    const value empty(object{});
    const auto append = [&](value row) {
        for (const auto& formula : formulas_) {
            charge(budget);
            object fields;
            for (const auto& dependency : formula.expression.dependencies()) {
                const auto* cell = row.find(dependency);
                if (!cell) unavailable();
                fields.emplace(dependency,*cell);
            }
            auto evaluated = formula.expression.evaluate(value(std::move(fields)),empty,budget,max_output_bytes - output_bytes);
            if (!scalar(evaluated) || !typed(formula.type,evaluated,budget)) unavailable();
            std::get<object>(row.data)[formula.alias] = std::move(evaluated);
        }
        if (!rows.empty()) {
            if (output_bytes == max_output_bytes) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
            ++output_bytes;
        }
        const auto encoded = json::encode(row,max_output_bytes-output_bytes);
        output_bytes += encoded.size();
        charge(budget,encoded.size());
        rows.push_back(std::move(row));
    };
    if (!groups_.empty() || !aggregates_.empty()) {
        struct bucket final { object row; std::vector<const value*> members; };
        std::vector<bucket> buckets;
        std::map<std::string,std::size_t> index;
        for (const auto& row : input) {
            object output; list key;
            for (const auto& group : groups_) {
                charge(budget);
                const auto* cell = row.find(group);
                if (!cell) unavailable();
                key.push_back(*cell); output[group] = *cell;
            }
            // Exact canonical tuple bytes are an injective key; a digest is unnecessary.
            const auto encoded = json::encode(value(std::move(key)), max_output_bytes);
            const auto [where, inserted] = index.emplace(encoded,buckets.size());
            if (inserted) buckets.push_back({std::move(output),{}});
            buckets[where->second].members.push_back(&row);
        }
        if (groups_.empty() && buckets.empty()) buckets.push_back({{}, {}});
        rows.reserve(buckets.size());
        for (auto& bucket : buckets) {
            for (const auto& aggregate : aggregates_) {
                charge(budget);
                if (aggregate.function == "count") {
                    bucket.row[aggregate.alias] = value(static_cast<std::int64_t>(bucket.members.size()));
                    continue;
                }
                std::size_t count = 0;
                value selected;
                auto sum = vm::decimal::parse("0");
                for (const auto* row : bucket.members) {
                    charge(budget);
                    const auto* cell = row->find(aggregate.column);
                    if (!cell) unavailable();
                    if (cell->is<std::nullptr_t>()) continue;
                    ++count;
                    if (aggregate.function == "min" || aggregate.function == "max") {
                        if (selected.is<std::nullptr_t>() || compare(*cell,selected,budget) * (aggregate.function == "min" ? 1 : -1) < 0) selected = *cell;
                    } else {
                        if (!cell->is<std::string>() && !cell->is<std::int64_t>()) unavailable();
                        const auto text = scalar_text(*cell);
                        charge(budget,text.size() + sum.literal().size());
                        sum = sum.add(vm::decimal::parse(text));
                    }
                }
                value computed;
                if (count != 0) {
                    if (aggregate.function == "min" || aggregate.function == "max") computed = selected.is<bool>() ? value(static_cast<std::int64_t>(selected.as<bool>())) : selected;
                    else if (aggregate.function == "avg") computed = value(sum.divide(vm::decimal::parse(std::to_string(count)),6,budget).literal());
                    else computed = value(sum.literal());
                }
                bucket.row[aggregate.alias] = std::move(computed);
            }
            append(value(std::move(bucket.row)));
        }
    } else for (const auto& row : input) append(row);
    if (!sorts_.empty()) {
        const value null;
        std::stable_sort(rows.begin(),rows.end(),[&](const value& left, const value& right) {
            for (const auto& sort : sorts_) {
                charge(budget);
                const auto* lp = left.find(sort.output); const auto* rp = right.find(sort.output);
                const auto& l = lp ? *lp : null; const auto& r = rp ? *rp : null;
                int ordering;
                if (l.is<std::nullptr_t>() || r.is<std::nullptr_t>()) {
                    ordering = l == r ? 0 : l.is<std::nullptr_t>() ? 1 : -1;
                    if (!sort.nulls_last) ordering = -ordering;
                } else ordering = compare(l,r,budget);
                // The frozen App applies descending direction after null placement.
                if (sort.descending) ordering = -ordering;
                if (ordering != 0) return ordering < 0;
            }
            return false;
        });
    }
    value result(std::move(rows));
    (void)json::encode(result,max_output_bytes);
    return result;
}
}
