#pragma once

#include "aethon/runtime/ingest_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::runtime {

enum class BatchCloseReason : std::uint8_t {
    none,
    max_packets,
    max_payload_bytes,
    max_age,
    deadline,
    priority_flush,
    manual_flush,
};

struct BatchingPolicyConfig {
    std::size_t max_packets = 64;
    std::size_t max_payload_bytes = 1024 * 1024;
    std::uint64_t max_age_ns = 5'000'000;
    std::uint64_t deadline_slack_ns = 0;
    IngestPriority priority_flush_threshold = IngestPriority::critical;
    bool isolate_control_packets = true;
    bool preserve_device_order = true;
};

struct PacketBatchWindow {
    std::uint64_t first_arrival_time_ns = 0;
    std::uint64_t last_arrival_time_ns = 0;
    std::uint64_t earliest_deadline_ns = 0;
};

struct PacketBatch {
    std::vector<IngestEnvelope> packets;
    PacketBatchWindow window;
    std::size_t payload_bytes = 0;
    IngestPriority highest_priority = IngestPriority::low;
    BatchCloseReason close_reason = BatchCloseReason::none;
};

struct BatchCandidate {
    bool accepted = false;
    BatchCloseReason close_reason = BatchCloseReason::none;
    std::string note;
};

struct BatchingDecision {
    std::vector<PacketBatch> ready_batches;
    std::optional<PacketBatch> open_batch;
};

struct BatchingPolicyStats {
    std::uint64_t packets_considered = 0;
    std::uint64_t packets_batched = 0;
    std::uint64_t batches_closed = 0;
    std::uint64_t manual_flushes = 0;
    std::uint64_t max_packet_closures = 0;
    std::uint64_t byte_limit_closures = 0;
    std::uint64_t age_closures = 0;
    std::uint64_t deadline_closures = 0;
    std::uint64_t priority_closures = 0;
};

class BatchingPolicy {
public:
    explicit BatchingPolicy(BatchingPolicyConfig config = {});

    [[nodiscard]] const BatchingPolicyConfig& config() const noexcept;
    [[nodiscard]] BatchingPolicyStats stats() const noexcept;
    [[nodiscard]] const std::optional<PacketBatch>& open_batch() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] std::vector<PacketBatch> offer(IngestEnvelope envelope, std::uint64_t now_ns);
    [[nodiscard]] std::vector<PacketBatch> offer_many(std::vector<IngestEnvelope> envelopes, std::uint64_t now_ns);
    [[nodiscard]] std::vector<PacketBatch> flush();
    [[nodiscard]] std::vector<PacketBatch> tick(std::uint64_t now_ns);
    void reset();

private:
    [[nodiscard]] BatchCandidate evaluate(const IngestEnvelope& envelope, std::uint64_t now_ns) const;
    [[nodiscard]] bool should_flush_open_batch(std::uint64_t now_ns) const;
    [[nodiscard]] bool conflicts_with_device_order(const IngestEnvelope& envelope) const;
    void append(IngestEnvelope envelope);
    [[nodiscard]] PacketBatch close(BatchCloseReason reason);
    void count_close(BatchCloseReason reason);

    BatchingPolicyConfig config_;
    std::optional<PacketBatch> open_;
    BatchingPolicyStats stats_;
};

[[nodiscard]] BatchCloseReason infer_batch_close_reason(const PacketBatch& batch,
                                                        const BatchingPolicyConfig& config,
                                                        std::uint64_t now_ns);
[[nodiscard]] std::string batch_close_reason_name(BatchCloseReason reason);
[[nodiscard]] std::string render_packet_batch(const PacketBatch& batch);
[[nodiscard]] std::string render_batching_stats(const BatchingPolicyStats& stats);

} // namespace aethon::runtime
