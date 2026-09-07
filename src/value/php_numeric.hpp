#pragma once
#include <string_view>

namespace kumwe::engine::value_compat {
// Frozen PHP string comparison fallback, after the exact-decimal report branch.
int php_string_compare(std::string_view left, std::string_view right);
}
