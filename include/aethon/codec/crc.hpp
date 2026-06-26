#pragma once

#include <cstdint>
#include <span>

namespace aethon::codec {

std::uint16_t crc16_ccitt(std::span<const std::uint8_t> data, std::uint16_t seed = 0xffff);
std::uint32_t crc32c(std::span<const std::uint8_t> data, std::uint32_t seed = 0xffffffffu);

} // namespace aethon::codec
