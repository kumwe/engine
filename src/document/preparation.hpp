#pragma once
#include "document/document.hpp"
#include "value/json.hpp"
#include "vm/formula.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace kumwe::engine::preparation {
struct field final {
    std::string handle;
    bool identity = false, sequence = false, computed = false;
    bool server_only = false, read_only = false, immutable = false;
    json::value default_value;
    bool default_valid = true;
    std::vector<vm::formula> visibility, editability;
};
// Pure caller-input preparation. Codec results and allocated identities/numbers
// are explicit host inputs; this class never normalizes or allocates them.
class plan final {
    std::vector<field> fields_;
    document::plan validation_;
    json::value document_;
public:
    static plan compile(const json::value& program);
    const json::value& document() const noexcept { return document_; }
    json::value execute(const json::value& request, const json::value& lines,
                        std::uint64_t& budget, std::size_t finding_limit,
                        std::size_t output_limit) const;
};
}
