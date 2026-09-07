#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace kumwe::engine::canonical {
// Incremental SHA-256; no payload-sized scratch allocation and no external crypto dependency.
class sha256 final {
    std::array<std::uint32_t, 8> state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                          0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::array<std::uint8_t, 64> block_{};
    std::uint64_t bytes_ = 0;
    std::size_t used_ = 0;
    void compress();
public:
    void update(std::string_view bytes);
    std::string finish();
};
}
