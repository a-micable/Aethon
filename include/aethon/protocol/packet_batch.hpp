#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::protocol {

struct PacketBatch {
    std::vector<Packet> packets;
};

struct PacketBatchSummary {
    std::uint64_t packet_count = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t first_timestamp_ns = 0;
    std::uint64_t last_timestamp_ns = 0;
};

void sort_batch_by_time(PacketBatch& batch);
[[nodiscard]] PacketBatchSummary summarize_batch(const PacketBatch& batch);
[[nodiscard]] PacketBatch filter_batch_by_device(const PacketBatch& batch, DeviceId device);
[[nodiscard]] PacketBatch filter_batch_by_kind(const PacketBatch& batch, PacketKind kind);
[[nodiscard]] std::string render_batch_summary(const PacketBatchSummary& summary);

} // namespace aethon::protocol
