#include "aethon/protocol/inspector.hpp"

#include <iomanip>
#include <sstream>

namespace aethon::protocol {
namespace {

std::string hex_id(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

void add_fragment_metadata(PacketInspection& inspection, const FragmentInfo& fragment) {
    std::ostringstream out;
    out << "fragment stream=" << fragment.stream_id
        << " sequence=" << fragment.sequence
        << " count=" << fragment.count;
    inspection.metadata.push_back(out.str());
    if (fragment.count == 0 || fragment.sequence >= fragment.count) {
        inspection.warnings.push_back("fragment coordinates are invalid");
    }
}

void add_route_metadata(PacketInspection& inspection, const RoutingInfo& route) {
    std::ostringstream out;
    out << "route region=" << route.region
        << " collector=" << route.collector
        << " priority=" << static_cast<unsigned>(route.priority);
    if (!route.relay_path.empty()) {
        out << " relays=";
        for (std::size_t i = 0; i < route.relay_path.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << route.relay_path[i];
        }
    }
    inspection.metadata.push_back(out.str());
}

void add_capability_metadata(PacketInspection& inspection, const DeviceCapabilities& caps) {
    std::ostringstream out;
    out << "capabilities max_payload=" << caps.max_payload
        << " compression=" << (caps.supports_compression ? "yes" : "no")
        << " encryption=" << (caps.supports_encryption ? "yes" : "no")
        << " gps_time=" << (caps.has_gps_time ? "yes" : "no");
    if (!caps.sensor_bands.empty()) {
        out << " bands=";
        for (std::size_t i = 0; i < caps.sensor_bands.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << caps.sensor_bands[i];
        }
    }
    inspection.metadata.push_back(out.str());
}

} // namespace

std::string version_name(ProtocolVersion version) {
    switch (version) {
    case ProtocolVersion::v1:
        return "v1";
    case ProtocolVersion::v2:
        return "v2";
    case ProtocolVersion::v3:
        return "v3";
    }
    return "unknown";
}

std::string packet_kind_name(PacketKind kind) {
    switch (kind) {
    case PacketKind::heartbeat:
        return "heartbeat";
    case PacketKind::observation:
        return "observation";
    case PacketKind::spectrum:
        return "spectrum";
    case PacketKind::control:
        return "control";
    case PacketKind::capabilities:
        return "capabilities";
    }
    return "unknown";
}

std::string compression_name(CompressionMode mode) {
    switch (mode) {
    case CompressionMode::none:
        return "none";
    case CompressionMode::rle:
        return "rle";
    }
    return "unknown";
}

std::string encryption_name(EncryptionMode mode) {
    switch (mode) {
    case EncryptionMode::none:
        return "none";
    case EncryptionMode::envelope:
        return "envelope";
    }
    return "unknown";
}

PacketInspection inspect_packet(const Packet& packet) {
    PacketInspection inspection;
    inspection.version = version_name(packet.version);
    inspection.kind = packet_kind_name(packet.kind);
    inspection.device = hex_id(packet.device);
    inspection.timestamp_ns = packet.timestamp_ns;
    inspection.sequence = packet.sequence;
    inspection.payload_size = packet.payload.size();

    inspection.metadata.push_back("compression=" + compression_name(packet.compression));
    inspection.metadata.push_back("encryption=" + encryption_name(packet.encryption));
    if (packet.fragment) {
        add_fragment_metadata(inspection, *packet.fragment);
    }
    if (packet.route) {
        add_route_metadata(inspection, *packet.route);
    }
    if (packet.capabilities) {
        add_capability_metadata(inspection, *packet.capabilities);
    }
    for (const auto& extension : packet.extensions) {
        std::ostringstream out;
        out << "extension type=" << extension.type << " bytes=" << extension.value.size();
        inspection.metadata.push_back(out.str());
    }
    if (packet.payload.empty() && packet.kind != PacketKind::heartbeat) {
        inspection.warnings.push_back("non-heartbeat packet has empty payload");
    }
    return inspection;
}

std::string render_packet_inspection(const PacketInspection& inspection) {
    std::ostringstream out;
    out << "packet " << inspection.kind << " " << inspection.version << "\n"
        << "  device: " << inspection.device << "\n"
        << "  timestamp_ns: " << inspection.timestamp_ns << "\n"
        << "  sequence: " << inspection.sequence << "\n"
        << "  payload_size: " << inspection.payload_size << "\n";
    for (const auto& item : inspection.metadata) {
        out << "  meta: " << item << "\n";
    }
    for (const auto& warning : inspection.warnings) {
        out << "  warning: " << warning << "\n";
    }
    return out.str();
}

} // namespace aethon::protocol
