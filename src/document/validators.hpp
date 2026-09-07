#pragma once
#include "value/json.hpp"
#include <memory>
#include <string>

namespace kumwe::engine::document {
struct pattern_code;
class validator final {
    json::value source_;
    std::shared_ptr<const pattern_code> pattern_;
public:
    explicit validator(json::value source);
    bool judge(const json::value& candidate, bool exact_decimal, bool domain_object, const json::value& definition) const;
    std::string name() const;
};
}
