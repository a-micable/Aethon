#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace aethon {

using Bytes = std::vector<std::uint8_t>;

inline std::string hex_byte(std::uint8_t value) {
    static constexpr char alphabet[] = "0123456789abcdef";
    std::string out(2, '0');
    out[0] = alphabet[(value >> 4) & 0x0f];
    out[1] = alphabet[value & 0x0f];
    return out;
}

std::string to_hex(std::span<const std::uint8_t> bytes, std::size_t limit = 64);

} // namespace aethon
