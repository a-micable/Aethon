#pragma once

#include "aethon/protocol/types.hpp"

#include <string>
#include <vector>

namespace aethon::protocol {

struct PacketInspection {
    std::string version;
    std::string kind;
    std::string device;
    std::uint64_t timestamp_ns = 0;
    std::uint32_t sequence = 0;
    std::size_t payload_size = 0;
    std::vector<std::string> metadata;
    std::vector<std::string> warnings;
};

std::string version_name(ProtocolVersion version);
std::string packet_kind_name(PacketKind kind);
std::string compression_name(CompressionMode mode);
std::string encryption_name(EncryptionMode mode);

PacketInspection inspect_packet(const Packet& packet);
std::string render_packet_inspection(const PacketInspection& inspection);

} // namespace aethon::protocol
