#include "canonical/sha256.hpp"
#include <bit>

namespace kumwe::engine::canonical {
namespace {
constexpr std::array<std::uint32_t, 64> constants = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
}
void sha256::compress() {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t i = 0; i < 16; ++i) {
        words[i] = (static_cast<std::uint32_t>(block_[i * 4]) << 24U)
            | (static_cast<std::uint32_t>(block_[i * 4 + 1]) << 16U)
            | (static_cast<std::uint32_t>(block_[i * 4 + 2]) << 8U)
            | static_cast<std::uint32_t>(block_[i * 4 + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const auto a = std::rotr(words[i - 15], 7) ^ std::rotr(words[i - 15], 18) ^ (words[i - 15] >> 3U);
        const auto b = std::rotr(words[i - 2], 17) ^ std::rotr(words[i - 2], 19) ^ (words[i - 2] >> 10U);
        words[i] = words[i - 16] + a + words[i - 7] + b;
    }
    auto a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    auto e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const auto t1 = h + s1 + ((e & f) ^ (~e & g)) + constants[i] + words[i];
        const auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const auto t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}
void sha256::update(std::string_view bytes) {
    bytes_ += static_cast<std::uint64_t>(bytes.size());
    for (const char byte : bytes) {
        block_[used_++] = static_cast<std::uint8_t>(byte);
        if (used_ == block_.size()) { compress(); used_ = 0; }
    }
}
std::string sha256::finish() {
    const auto bits = bytes_ * 8U;
    block_[used_++] = 0x80U;
    if (used_ > 56) {
        while (used_ < 64) block_[used_++] = 0;
        compress(); used_ = 0;
    }
    while (used_ < 56) block_[used_++] = 0;
    for (unsigned i = 0; i < 8; ++i) block_[63U - i] = static_cast<std::uint8_t>(bits >> (i * 8U));
    compress();
    constexpr std::string_view hex = "0123456789abcdef";
    std::string output;
    output.reserve(64);
    for (const auto word : state_) {
        for (unsigned i = 0; i < 8; ++i) output += hex[(word >> ((7U - i) * 4U)) & 15U];
    }
    return output;
}
}
