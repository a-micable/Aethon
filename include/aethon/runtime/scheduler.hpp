#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/runtime/ingest_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::runtime {

using SchedulerTaskId = std::uint64_t;

enum class SchedulerTaskKind : std::uint8_t {
    ingest,
    batch,
    dispatch,
    maintenance,
    control,
};

enum class SchedulerTaskState : std::uint8_t {
    queued,
    running,
    completed,
    cancelled,
    expired,
};

enum class SchedulerPickReason : std::uint8_t {
    none,
    ready,
    deadline,
    priority,
    fairness,
};

struct SchedulerTask {
    SchedulerTaskId id = 0;
    SchedulerTaskKind kind = SchedulerTaskKind::maintenance;
    protocol::DeviceId device = 0;
    std::uint64_t enqueue_time_ns = 0;
    std::uint64_t ready_time_ns = 0;
    std::uint64_t deadline_ns = 0;
    IngestPriority priority = IngestPriority::normal;
    std::uint32_t cost = 1;
    std::string label;
};

struct SchedulerLease {
    SchedulerTask task;
    SchedulerPickReason reason = SchedulerPickReason::none;
    std::uint64_t lease_time_ns = 0;
};

struct SchedulerCompletion {
    SchedulerTaskId id = 0;
    bool success = true;
    std::uint64_t complete_time_ns = 0;
    std::uint64_t retry_ready_time_ns = 0;
    std::string note;
};

struct SchedulerStats {
    std::uint64_t enqueued = 0;
    std::uint64_t leased = 0;
    std::uint64_t completed = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t expired = 0;
    std::uint64_t retried = 0;
    std::uint64_t duplicate_rejections = 0;
};

struct SchedulerConfig {
    std::size_t max_tasks = 4096;
    std::uint32_t per_device_burst = 8;
    std::uint64_t fairness_window_ns = 1'000'000;
    bool reject_duplicate_labels = true;
};

struct SchedulerSnapshotTask {
    SchedulerTask task;
    SchedulerTaskState state = SchedulerTaskState::queued;
    std::uint32_t attempts = 0;
    std::uint64_t last_lease_time_ns = 0;
    std::vector<std::string> notes;
};

struct SchedulerSnapshot {
    SchedulerStats stats;
    std::size_t queued = 0;
    std::size_t running = 0;
    std::vector<SchedulerSnapshotTask> tasks;
};

class RuntimeScheduler {
public:
    explicit RuntimeScheduler(SchedulerConfig config = {});

    [[nodiscard]] const SchedulerConfig& config() const noexcept;
    [[nodiscard]] SchedulerStats stats() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] SchedulerSnapshot snapshot() const;

    [[nodiscard]] SchedulerTaskId enqueue(SchedulerTask task);
    [[nodiscard]] std::optional<SchedulerLease> lease_next(std::uint64_t now_ns);
    [[nodiscard]] std::vector<SchedulerLease> lease_many(std::uint64_t now_ns, std::size_t max_count);
    [[nodiscard]] bool complete(SchedulerCompletion completion);
    [[nodiscard]] bool cancel(SchedulerTaskId id, std::string note);
    [[nodiscard]] std::vector<SchedulerTask> expire(std::uint64_t now_ns);
    void clear_completed();
    void reset();

private:
    struct TaskRecord {
        SchedulerTask task;
        SchedulerTaskState state = SchedulerTaskState::queued;
        std::uint32_t attempts = 0;
        std::uint64_t last_lease_time_ns = 0;
        std::vector<std::string> notes;
    };

    [[nodiscard]] SchedulerTaskId next_id();
    [[nodiscard]] bool duplicate_label(const SchedulerTask& task) const;
    [[nodiscard]] std::map<SchedulerTaskId, TaskRecord>::iterator select(std::uint64_t now_ns);
    [[nodiscard]] SchedulerPickReason pick_reason(const TaskRecord& record, std::uint64_t now_ns) const;
    [[nodiscard]] bool device_burst_available(protocol::DeviceId device) const;
    void record_lease(TaskRecord& record, std::uint64_t now_ns);
    void release_device(protocol::DeviceId device);
    void note(TaskRecord& record, std::string message);

    SchedulerConfig config_;
    std::map<SchedulerTaskId, TaskRecord> tasks_;
    std::map<protocol::DeviceId, std::uint32_t> running_by_device_;
    SchedulerStats stats_;
    SchedulerTaskId next_id_ = 1;
};

[[nodiscard]] std::string scheduler_task_kind_name(SchedulerTaskKind kind);
[[nodiscard]] std::string scheduler_task_state_name(SchedulerTaskState state);
[[nodiscard]] std::string scheduler_pick_reason_name(SchedulerPickReason reason);
[[nodiscard]] std::string render_scheduler_snapshot(const SchedulerSnapshot& snapshot);

} // namespace aethon::runtime
