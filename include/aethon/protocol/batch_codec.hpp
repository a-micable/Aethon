#pragma once

#include "aethon/protocol/packet_batch.hpp"

#include <cstdint>
#include <span>

namespace aethon::protocol {

[[nodiscard]] Bytes encode_packet_batch(const PacketBatch& batch);
[[nodiscard]] PacketBatch decode_packet_batch(std::span<const std::uint8_t> bytes);

} // namespace aethon::protocol
