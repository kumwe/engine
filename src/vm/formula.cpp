#include "formula.hpp"
#include "decimal.hpp"
#include "error.hpp"
#include "batch.hpp"
#include "value/php_numeric.hpp"
#include <kumwe/engine/engine.h>
#include <algorithm>
#include <limits>
#include <set>

namespace kumwe::engine::vm {
namespace {
using value = json::value;
using object = value::object;
using list = value::list;
[[noreturn]] void invalid(const std::string& message) { throw error(true, message); }
const value null;
const value& member(const value& data, std::string_view key) { const auto* found = data.find(key); return found ? *found : null; }
std::string text(const value& data) { return data.is<std::string>() ? data.as<std::string>() : ""; }
bool one_of(std::string_view text, std::initializer_list<std::string_view> set) { return std::find(set.begin(), set.end(), text) != set.end(); }
bool handle(std::string_view text) {
    if (text.empty() || text.size() > 63 || text.front() < 'a' || text.front() > 'z') return false;
    for (char c : text) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
bool keys(const value& node, std::initializer_list<std::string_view> allowed) {
    if (!node.is<object>()) return false;
    for (const auto& [key, unused] : node.as<object>()) { (void)unused; if (!one_of(key, allowed)) return false; }
    return true;
}
bool typed(std::string_view type, const value& input, bool literal) {
    if (type == "null") return input.is<std::nullptr_t>();
    if (type == "boolean") return input.is<bool>();
    if (type == "integer") return input.is<std::int64_t>();
    if (type == "decimal") return input.is<std::string>() && decimal_literal(input.as<std::string>());
    if (one_of(type, {"string","date","time","datetime"})) return input.is<std::string>() && (!literal || input.as<std::string>().size() <= 4096);
    if (type == "any") return input.is<std::nullptr_t>() || input.is<bool>() || input.is<std::int64_t>() || input.is<std::string>();
    return false;
}
void safe_definition(const value& input, std::size_t depth) {
    if (depth > 32) invalid("A business definition exceeds the maximum nesting depth.");
    if (input.is<json::number>()) invalid("Business definitions cannot contain floats, resources, or objects.");
    if (input.is<list>()) {
        if (input.as<list>().size() > 512) invalid("A business-definition collection exceeds 512 entries.");
        for (const auto& child : input.as<list>()) safe_definition(child, depth + 1);
    } else if (input.is<object>()) {
        if (input.as<object>().size() > 512) invalid("A business-definition collection exceeds 512 entries.");
        for (const auto& [key, child] : input.as<object>()) { (void)key; safe_definition(child, depth + 1); }
    }
}
const std::map<std::string_view, std::pair<std::size_t, std::size_t>> arity = {
    {"literal",{0,0}}, {"field",{0,0}}, {"line_aggregate",{0,0}},
    {"eq",{2,2}}, {"ne",{2,2}}, {"lt",{2,2}}, {"lte",{2,2}}, {"gt",{2,2}}, {"gte",{2,2}},
    {"and",{2,16}}, {"or",{2,16}}, {"not",{1,1}}, {"add",{2,16}}, {"subtract",{2,2}},
    {"multiply",{2,16}}, {"divide",{2,2}}, {"concat",{2,16}}, {"coalesce",{2,16}},
    {"if",{3,3}}, {"is_null",{1,1}}, {"in",{2,32}}, {"contains",{2,2}}
};
bool compatible(std::string_view output, std::string_view argument) { return output == "any" || output == argument || argument == "null"; }
void check_types(const instruction& node, const std::vector<instruction>& code) {
    const auto& op = node.operation; const auto& type = node.type;
    auto argument_type = [&](std::size_t i) -> const std::string& { return code[node.arguments[i]].type; };
    if (one_of(op,{"eq","ne","lt","lte","gt","gte","and","or","not","is_null","in","contains"}) && type != "boolean")
        invalid("Expression operator " + op + " must produce boolean.");
    if (one_of(op,{"and","or","not"})) for (const auto arg : node.arguments)
        if (code[arg].type != "boolean") invalid("Expression operator " + op + " requires boolean arguments.");
    if (one_of(op,{"add","subtract","multiply","divide"})) {
        if (!one_of(type,{"integer","decimal"})) invalid("Expression operator " + op + " requires a numeric result.");
        for (const auto arg : node.arguments) if (code[arg].type != type) invalid("Expression operator " + op + " has incompatible types.");
    }
    if (one_of(op,{"eq","ne","lt","lte","gt","gte","in"}))
        for (const auto arg : node.arguments) if (code[arg].type != argument_type(0)) invalid("Expression operator " + op + " has incompatible argument types.");
    if (one_of(op,{"lt","lte","gt","gte"}) && !one_of(argument_type(0),{"integer","decimal","string","date","time","datetime"}))
        invalid("Ordered comparison arguments have an unsupported type.");
    if (op == "contains" || op == "concat") {
        if (type != (op == "contains" ? "boolean" : "string")) invalid("Expression operator " + op + " has an incompatible result type.");
        for (const auto arg : node.arguments) if (code[arg].type != "string") invalid("Expression operator " + op + " has incompatible argument types.");
    }
    if (op == "if" && (argument_type(0) != "boolean" || !compatible(type, argument_type(1)) || !compatible(type, argument_type(2))))
        invalid("Expression operator if has incompatible argument types.");
    if (op == "coalesce") for (const auto arg : node.arguments)
        if (!compatible(type, code[arg].type)) invalid("Expression operator coalesce has incompatible types.");
}
std::size_t parse_node(const value& document, std::vector<instruction>& code, std::size_t depth, std::size_t& count, value& canonical) {
    if (depth > 12) invalid("A condition or formula exceeds 12 expression levels.");
    if (!keys(document,{"op","type","args","value","field","scale","lines","aggregate"})) invalid("A condition or formula contains an unknown property.");
    instruction node;
    node.operation = text(member(document,"op")); node.type = text(member(document,"type"));
    const auto& op = node.operation; const auto& type = node.type;
    if (!arity.contains(op)) invalid("A condition or formula operator is unsupported.");
    if (!one_of(type,{"any","null","boolean","integer","decimal","string","date","time","datetime"})) invalid("A condition or formula type is unsupported.");
    if (++count > 128) invalid("A condition or formula exceeds 128 operations.");
    object result{{"op",value(op)},{"type",value(type)}};
    if (op == "literal") {
        if (!keys(document,{"op","type","value"}) || !document.find("value")) invalid("A literal expression has an invalid shape.");
        node.literal = document.at("value");
        if (!typed(type,node.literal,true)) invalid("An expression literal does not match its declared type.");
        result.emplace("value",node.literal);
    } else if (op == "field") {
        if (!keys(document,{"op","type","field"})) invalid("A field expression has an invalid shape.");
        node.field = text(member(document,"field"));
        if (!handle(node.field)) invalid("A field expression references an invalid field.");
        result.emplace("field",value(node.field));
    } else if (op == "line_aggregate") {
        if (!keys(document,{"op","type","lines","aggregate","field"})) invalid("A line aggregation has an invalid shape.");
        node.aggregate = text(member(document,"aggregate")); node.lines = text(member(document,"lines"));
        if (!one_of(node.aggregate,{"count","sum"})) invalid("A line aggregation names an unsupported reduction.");
        if (!handle(node.lines)) invalid("A line aggregation references an invalid owned-line collection.");
        if (!(type == "integer" || (node.aggregate == "sum" && type == "decimal"))) invalid("Line aggregation " + node.aggregate + " cannot produce " + type + ".");
        const auto& field = member(document,"field");
        if (node.aggregate == "count") {
            if (!field.is<std::nullptr_t>()) invalid("A line count measures the collection and takes no field.");
        } else {
            node.field = text(field); if (!handle(node.field)) invalid("A line aggregation references an invalid line field.");
            result.emplace("field",value(node.field));
        }
        result.emplace("lines",value(node.lines)); result.emplace("aggregate",value(node.aggregate));
    } else {
        if (!keys(document,{"op","type","args","scale"})) invalid("An operator expression has an invalid shape.");
        const auto& arguments = member(document,"args");
        if (!arguments.is<list>()) invalid("An operator expression requires an argument list.");
        const auto [minimum,maximum] = arity.at(op);
        if (arguments.as<list>().size() < minimum || arguments.as<list>().size() > maximum) invalid("Expression operator " + op + " has an invalid arity.");
        list children;
        for (const auto& child : arguments.as<list>()) {
            if (!child.is<object>() || child.as<object>().empty()) invalid("Every expression argument must be an object.");
            value normalized;
            node.arguments.push_back(parse_node(child,code,depth+1,count,normalized));
            children.push_back(std::move(normalized));
        }
        result.emplace("args",value(std::move(children)));
        const auto& scale = member(document,"scale");
        if (!scale.is<std::nullptr_t>()) {
            if (!scale.is<std::int64_t>() || scale.as<std::int64_t>() < 0 || scale.as<std::int64_t>() > 30 || op != "divide" || type != "decimal")
                invalid("Expression scale is supported only for decimal division.");
            node.scale = static_cast<std::size_t>(scale.as<std::int64_t>()); result.emplace("scale",scale);
        }
        if (op == "divide" && type == "decimal" && !scale.is<std::int64_t>()) invalid("Decimal division requires an explicit output scale.");
        check_types(node,code);
    }
    canonical = value(std::move(result)); code.push_back(std::move(node)); return code.size()-1;
}
std::int64_t integer(const value& input) { if (!input.is<std::int64_t>()) reject("A formula expected an integer value."); return input.as<std::int64_t>(); }
bool boolean(const value& input) { if (!input.is<bool>()) reject("A formula expected a boolean value."); return input.as<bool>(); }
const std::string& string(const value& input) { if (!input.is<std::string>()) reject("A formula expected a string value."); return input.as<std::string>(); }
void charge(std::uint64_t& budget, std::uint64_t cost = 1) {
    if (cost > budget) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
    budget -= cost;
}
std::int64_t arithmetic(std::string_view operation, std::int64_t left, std::int64_t right, bool line = false) {
    std::int64_t result = 0; bool overflow = false;
    if (operation == "add") overflow = __builtin_add_overflow(left,right,&result);
    else if (operation == "subtract") overflow = __builtin_sub_overflow(left,right,&result);
    else if (operation == "multiply") overflow = __builtin_mul_overflow(left,right,&result);
    else {
        if (right == 0) reject("A formula attempted division by zero.");
        overflow = left == std::numeric_limits<std::int64_t>::min() && right == -1;
        if (!overflow) result = left / right;
    }
    if (overflow) reject(line ? "A line sum exceeded the platform integer range." : "An integer formula exceeded the platform integer range.");
    return result;
}
int compare(std::string_view type, const value& left, const value& right) {
    if (type == "decimal") return decimal::parse(string(left)).compare(decimal::parse(string(right)));
    if (left.is<std::int64_t>() && right.is<std::int64_t>()) return left == right ? 0 : left.as<std::int64_t>() < right.as<std::int64_t>() ? -1 : 1;
    if (left.is<std::string>() && right.is<std::string>()) {
        const auto& l = left.as<std::string>(); const auto& r = right.as<std::string>();
        return value_compat::php_string_compare(l, r);
    }
    reject("Formula comparison operands are incompatible.");
}
bool equal(std::string_view type, const value& left, const value& right) {
    if (type != "decimal" || left.is<std::nullptr_t>() || right.is<std::nullptr_t>()) return left == right;
    return compare(type,left,right) == 0;
}
}
formula formula::compile(const json::value& expression) {
    safe_definition(expression,0);
    if (json::encode(expression).size() > 32768) invalid("A condition or formula exceeds 32768 canonical bytes.");
    formula result; std::size_t count = 0;
    (void)parse_node(expression,result.code_,1,count,result.document_);
    std::set<std::string> dependencies;
    std::map<std::string,std::set<std::string>> lines;
    for (const auto& instruction : result.code_) {
        if (instruction.operation == "field") dependencies.emplace(instruction.field);
        if (instruction.operation == "line_aggregate") {
            auto& fields = lines[instruction.lines]; if (!instruction.field.empty()) fields.emplace(instruction.field);
        }
    }
    result.dependencies_ = {dependencies.begin(),dependencies.end()};
    for (const auto& [collection,fields] : lines) result.lines_.emplace(collection,std::vector<std::string>(fields.begin(),fields.end()));
    return result;
}
json::value formula::evaluate(const json::value& fields, const json::value& lines, std::uint64_t& budget, std::size_t output_limit) const {
    std::vector<value> values; values.reserve(code_.size());
    for (const auto& node : code_) {
        charge(budget);
        const auto& op = node.operation; const auto& type = node.type;
        auto arg = [&](std::size_t i) -> const value& { return values[node.arguments[i]]; };
        value result;
        if (op == "literal") result = node.literal;
        else if (op == "field") {
            const auto* found = fields.find(node.field);
            if (!found) reject("A formula dependency is unavailable.");
            if (found->is<json::number>()) reject("Formula inputs cannot contain PHP floats.");
            if (!typed(type,*found,false)) reject("A formula field value does not match its declared type.");
            result = *found;
        } else if (op == "line_aggregate") {
            const auto* collection = lines.find(node.lines);
            if (!collection) reject("An owned-line collection an invariant reduces was not supplied.");
            if (!collection->is<list>()) throw refusal(KUMWE_ENGINE_V1_INVALID_INPUT);
            if (op == "count" || node.aggregate == "count") result = value(static_cast<std::int64_t>(collection->as<list>().size()));
            else if (type == "integer") {
                std::int64_t total = 0;
                for (const auto& line : collection->as<list>()) {
                    charge(budget); const auto& item = member(line,node.field);
                    if (!item.is<std::nullptr_t>()) total = arithmetic("add",total,integer(item),true);
                }
                result = value(total);
            } else {
                auto total = decimal::parse("0");
                for (const auto& line : collection->as<list>()) {
                    charge(budget); const auto& item = member(line,node.field); if (item.is<std::nullptr_t>()) continue;
                    if (!item.is<std::string>() || !decimal_literal(item.as<std::string>())) reject("A line sum read a value that is not an exact decimal.");
                    total = total.add(decimal::parse(item.as<std::string>()));
                }
                result = value(total.literal());
            }
        } else if (op == "eq" || op == "ne") {
            const bool same = equal(code_[node.arguments[0]].type,arg(0),arg(1)); result = value(op == "eq" ? same : !same);
        } else if (one_of(op,{"lt","lte","gt","gte"})) {
            const int compared = compare(code_[node.arguments[0]].type,arg(0),arg(1));
            result = value(op == "lt" ? compared < 0 : op == "lte" ? compared <= 0 : op == "gt" ? compared > 0 : compared >= 0);
        } else if (op == "and" || op == "or") {
            bool combined = op == "and";
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                if (op == "and" && arg(i).is<bool>() && !arg(i).as<bool>()) combined = false;
                if (op == "or" && arg(i).is<bool>() && arg(i).as<bool>()) combined = true;
            }
            result = value(combined);
        } else if (op == "not") result = value(!boolean(arg(0)));
        else if (one_of(op,{"add","subtract","multiply","divide"})) {
            if (type == "integer") {
                auto total = integer(arg(0));
                for (std::size_t i = 1; i < node.arguments.size(); ++i) total = arithmetic(op,total,integer(arg(i)));
                result = value(total);
            } else {
                auto total = decimal::parse(string(arg(0)));
                for (std::size_t i = 1; i < node.arguments.size(); ++i) {
                    const auto operand = decimal::parse(string(arg(i)));
                    total = op == "add" ? total.add(operand) : op == "subtract" ? total.subtract(operand)
                        : op == "multiply" ? total.multiply(operand,budget) : total.divide(operand,node.scale,budget);
                }
                result = value(total.literal());
            }
        } else if (op == "concat") {
            std::string joined;
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                const auto& part = string(arg(i));
                if (part.size() > output_limit - joined.size()) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
                joined += part;
            }
            result = value(std::move(joined));
        } else if (op == "coalesce") {
            for (std::size_t i = 0; i < node.arguments.size(); ++i) if (!arg(i).is<std::nullptr_t>()) { result = arg(i); break; }
        } else if (op == "if") result = boolean(arg(0)) ? arg(1) : arg(2);
        else if (op == "is_null") result = value(arg(0).is<std::nullptr_t>());
        else if (op == "contains") result = value(string(arg(0)).find(string(arg(1))) != std::string::npos);
        else if (op == "in") {
            bool found = false;
            for (std::size_t i = 1; i < node.arguments.size(); ++i) if (equal(code_[node.arguments[0]].type,arg(0),arg(i))) { found = true; break; }
            result = value(found);
        } else reject("A formula operator is not executable.");
        if (result.is<std::string>() && result.as<std::string>().size() > output_limit) throw refusal(KUMWE_ENGINE_V1_EXHAUSTED_LIMIT);
        values.push_back(std::move(result));
    }
    return values.back();
}
}
