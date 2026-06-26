#pragma once
#include "aethon/common/bytes.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace aethon::protocol {
using DeviceId = std::uint64_t;
enum class ProtocolVersion : std::uint8_t { v1 = 1, v2 = 2, v3 = 3 };
enum class PacketKind : std::uint8_t { heartbeat = 1, observation = 2, spectrum = 3, control = 4, capabilities = 5 };
enum class CompressionMode : std::uint8_t { none = 0, rle = 1 };
enum class EncryptionMode : std::uint8_t { none = 0, envelope = 1 };
struct FragmentInfo { std::uint32_t stream_id = 0; std::uint16_t sequence = 0; std::uint16_t count = 1; };
struct RoutingInfo { std::uint16_t region = 0; std::uint16_t collector = 0; std::uint8_t priority = 0; std::vector<std::uint16_t> relay_path; };
struct DeviceCapabilities { bool supports_compression = false; bool supports_encryption = false; bool has_gps_time = false; std::uint16_t max_payload = 4096; std::vector<std::string> sensor_bands; };
struct ExtensionHeader { std::uint16_t type = 0; Bytes value; };
struct Packet { ProtocolVersion version = ProtocolVersion::v3; PacketKind kind = PacketKind::observation; DeviceId device = 0; std::uint64_t timestamp_ns = 0; std::uint32_t sequence = 0; CompressionMode compression = CompressionMode::none; EncryptionMode encryption = EncryptionMode::none; std::optional<FragmentInfo> fragment; std::optional<RoutingInfo> route; std::optional<DeviceCapabilities> capabilities; std::vector<ExtensionHeader> extensions; Bytes payload; };
struct NegotiatedProtocol { ProtocolVersion selected = ProtocolVersion::v1; bool compression = false; bool encryption = false; };
NegotiatedProtocol negotiate(ProtocolVersion local_max, const DeviceCapabilities& remote);
} // namespace aethon::protocol
