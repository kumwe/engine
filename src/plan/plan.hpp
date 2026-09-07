#pragma once
#include "document/document.hpp"
#include "vm/formula.hpp"
#include "reporting/report.hpp"
#include "value/json.hpp"
#include <atomic>
#include <string>
#include <variant>

namespace kumwe::engine::execution {
class plan final {
    std::variant<vm::formula, document::plan, reporting::report_plan> implementation_;
    json::value descriptor_;
public:
    static plan compile(std::string_view request);
    std::string describe() const;
    std::string execute(std::string_view request, const std::atomic<bool>* cancellation) const;
};
}
