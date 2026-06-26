#pragma once
#include "aethon/protocol/types.hpp"
#include <span>
namespace aethon::protocol {
inline constexpr std::uint32_t packet_magic = 0x4e485441;
struct DecodeOptions { std::size_t max_packet_size = 1024 * 1024; bool require_known_extensions = false; };
Bytes encode_packet(const Packet& packet);
Packet decode_packet(std::span<const std::uint8_t> frame, DecodeOptions options = {});
Bytes encode_payload_sections(const Packet& packet);
void decode_payload_sections(Packet& packet, std::span<const std::uint8_t> sections, DecodeOptions options);
} // namespace aethon::protocol
