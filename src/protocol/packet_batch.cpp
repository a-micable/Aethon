#include "aethon/protocol/packet_batch.hpp"

#include "aethon/protocol/inspector.hpp"

#include <algorithm>
#include <sstream>

namespace aethon::protocol {

void sort_batch_by_time(PacketBatch& batch) {
    std::sort(
        batch.packets.begin(),
        batch.packets.end(),
        [](const Packet& left, const Packet& right) {
            if (left.timestamp_ns != right.timestamp_ns) {
                return left.timestamp_ns < right.timestamp_ns;
            }
            if (left.device != right.device) {
                return left.device < right.device;
            }
            return left.sequence < right.sequence;
        });
}

PacketBatchSummary summarize_batch(const PacketBatch& batch) {
    PacketBatchSummary summary;
    summary.packet_count = batch.packets.size();
    bool have_time = false;
    for (const auto& packet : batch.packets) {
        summary.payload_bytes += packet.payload.size();
        if (!have_time) {
            summary.first_timestamp_ns = packet.timestamp_ns;
            summary.last_timestamp_ns = packet.timestamp_ns;
            have_time = true;
        } else {
            summary.first_timestamp_ns = std::min(summary.first_timestamp_ns, packet.timestamp_ns);
            summary.last_timestamp_ns = std::max(summary.last_timestamp_ns, packet.timestamp_ns);
        }
    }
    return summary;
}

PacketBatch filter_batch_by_device(const PacketBatch& batch, DeviceId device) {
    PacketBatch filtered;
    for (const auto& packet : batch.packets) {
        if (packet.device == device) {
            filtered.packets.push_back(packet);
        }
    }
    return filtered;
}

PacketBatch filter_batch_by_kind(const PacketBatch& batch, PacketKind kind) {
    PacketBatch filtered;
    for (const auto& packet : batch.packets) {
        if (packet.kind == kind) {
            filtered.packets.push_back(packet);
        }
    }
    return filtered;
}

std::string render_batch_summary(const PacketBatchSummary& summary) {
    std::ostringstream out;
    out << "packet_batch\n"
        << "  packets: "
        << summary.packet_count
        << "\n"
        << "  payload_bytes: "
        << summary.payload_bytes
        << "\n"
        << "  first_timestamp_ns: "
        << summary.first_timestamp_ns
        << "\n"
        << "  last_timestamp_ns: "
        << summary.last_timestamp_ns
        << "\n";
    return out.str();
}

} // namespace aethon::protocol
