#include "aethon/codec/crc.hpp"

namespace aethon::codec {

std::uint16_t crc16_ccitt(std::span<const std::uint8_t> data, std::uint16_t seed) {
    auto crc = seed;
    for (auto byte : data) {
        crc ^= static_cast<std::uint16_t>(byte) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

std::uint32_t crc32c(std::span<const std::uint8_t> data, std::uint32_t seed) {
    auto crc = ~seed;
    for (auto byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0x82f63b78u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

} // namespace aethon::codec
