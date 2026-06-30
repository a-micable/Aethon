#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/runtime/event_dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::runtime {

enum class IngestPriority : std::uint8_t {
    low = 0,
    normal = 1,
    high = 2,
    critical = 3,
};

enum class IngestAdmissionStatus : std::uint8_t {
    accepted,
    rejected_closed,
    rejected_empty_payload,
    rejected_payload_too_large,
    rejected_queue_full,
    replaced_older_packet,
};

enum class IngestOverflowPolicy : std::uint8_t {
    reject_new,
    drop_oldest,
    drop_lowest_priority,
    replace_same_device,
};

enum class IngestDrainMode : std::uint8_t {
    fifo,
    highest_priority_first,
    earliest_deadline_first,
};

struct IngestQueueConfig {
    std::size_t max_packets = 1024;
    std::size_t max_payload_bytes = 16 * 1024 * 1024;
    std::size_t max_packet_payload_bytes = 256 * 1024;
    IngestOverflowPolicy overflow_policy = IngestOverflowPolicy::reject_new;
    IngestDrainMode drain_mode = IngestDrainMode::fifo;
    bool reject_empty_payload = false;
    bool publish_events = true;
};

struct IngestEnvelope {
    protocol::Packet packet;
    std::uint64_t arrival_time_ns = 0;
    std::uint64_t deadline_ns = 0;
    IngestPriority priority = IngestPriority::normal;
    std::string source;
};

struct IngestAdmission {
    IngestAdmissionStatus status = IngestAdmissionStatus::accepted;
    std::string reason;
    std::optional<protocol::Packet> evicted;
    std::size_t queue_size = 0;
    std::size_t payload_bytes = 0;
};

struct IngestDrainResult {
    std::vector<IngestEnvelope> packets;
    std::size_t remaining_packets = 0;
    std::size_t remaining_payload_bytes = 0;
};

struct IngestDeviceSnapshot {
    protocol::DeviceId device = 0;
    std::size_t queued_packets = 0;
    std::size_t queued_payload_bytes = 0;
    std::uint64_t newest_sequence = 0;
    std::uint64_t last_arrival_time_ns = 0;
    IngestPriority highest_priority = IngestPriority::low;
};

struct IngestQueueStats {
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t evicted = 0;
    std::uint64_t drained = 0;
    std::uint64_t closed_rejections = 0;
    std::uint64_t payload_rejections = 0;
    std::uint64_t capacity_rejections = 0;
    std::size_t queued_packets = 0;
    std::size_t queued_payload_bytes = 0;
    std::size_t high_water_packets = 0;
    std::size_t high_water_payload_bytes = 0;
};

class IngestQueue {
public:
    explicit IngestQueue(IngestQueueConfig config = {});

    void subscribe(RuntimeEventHandler handler);
    void close();
    void reopen();
    void clear();

    [[nodiscard]] bool closed() const noexcept;
    [[nodiscard]] const IngestQueueConfig& config() const noexcept;
    [[nodiscard]] IngestQueueStats stats() const noexcept;
    [[nodiscard]] std::vector<IngestDeviceSnapshot> devices() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t payload_bytes() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] IngestAdmission push(IngestEnvelope envelope);
    [[nodiscard]] std::optional<IngestEnvelope> peek() const;
    [[nodiscard]] std::optional<IngestEnvelope> pop();
    [[nodiscard]] IngestDrainResult drain(std::size_t max_packets);
    [[nodiscard]] IngestDrainResult drain_payload_budget(std::size_t max_payload_bytes);

private:
    struct DeviceState {
        std::size_t queued_packets = 0;
        std::size_t queued_payload_bytes = 0;
        std::uint64_t newest_sequence = 0;
        std::uint64_t last_arrival_time_ns = 0;
        IngestPriority highest_priority = IngestPriority::low;
    };

    [[nodiscard]] IngestAdmission reject(IngestAdmissionStatus status, std::string reason) const;
    [[nodiscard]] bool would_exceed_capacity(const IngestEnvelope& envelope) const noexcept;
    [[nodiscard]] std::deque<IngestEnvelope>::iterator select_overflow_victim(const IngestEnvelope& incoming);
    [[nodiscard]] std::deque<IngestEnvelope>::iterator select_next();
    [[nodiscard]] std::deque<IngestEnvelope>::const_iterator select_next() const;
    void insert(IngestEnvelope envelope);
    void remove_at(std::deque<IngestEnvelope>::iterator iter);
    void add_device_state(const IngestEnvelope& envelope);
    void subtract_device_state(const IngestEnvelope& envelope);
    void rebuild_device_state(protocol::DeviceId device);
    void update_high_water();
    void publish(RuntimeEventKind kind, const IngestEnvelope& envelope, std::string message) const;

    IngestQueueConfig config_;
    std::deque<IngestEnvelope> queue_;
    std::map<protocol::DeviceId, DeviceState> devices_;
    IngestQueueStats stats_;
    EventDispatcher dispatcher_;
    bool closed_ = false;
};

[[nodiscard]] IngestPriority infer_ingest_priority(const protocol::Packet& packet);
[[nodiscard]] std::string ingest_priority_name(IngestPriority priority);
[[nodiscard]] std::string ingest_admission_status_name(IngestAdmissionStatus status);
[[nodiscard]] std::string ingest_overflow_policy_name(IngestOverflowPolicy policy);
[[nodiscard]] std::string ingest_drain_mode_name(IngestDrainMode mode);
[[nodiscard]] std::string render_ingest_admission(const IngestAdmission& admission);
[[nodiscard]] std::string render_ingest_queue_stats(const IngestQueueStats& stats);

} // namespace aethon::runtime
