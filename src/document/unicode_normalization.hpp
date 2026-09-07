#ifndef KUMWE_ENGINE_DOCUMENT_UNICODE_NORMALIZATION_HPP
#define KUMWE_ENGINE_DOCUMENT_UNICODE_NORMALIZATION_HPP
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace kumwe::engine::document {
// Internal field normalizers: PHP 8.5 Unicode 17 default casing and ICU 74 Unicode 15.1 NFC.
std::string normalize_unicode(std::string_view text, std::string_view operation,
                              std::uint64_t& budget, std::size_t output_limit);
}
#endif
