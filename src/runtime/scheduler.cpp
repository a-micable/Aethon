#include "aethon/runtime/scheduler.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aethon::runtime {
namespace {

std::uint8_t priority_score(IngestPriority priority) {
    return static_cast<std::uint8_t>(priority);
}

bool is_terminal(SchedulerTaskState state) {
    return state == SchedulerTaskState::completed
        || state == SchedulerTaskState::cancelled
        || state == SchedulerTaskState::expired;
}

bool has_deadline(const SchedulerTask& task) {
    return task.deadline_ns != 0;
}

} // namespace

RuntimeScheduler::RuntimeScheduler(SchedulerConfig config)
    : config_(config) {}

const SchedulerConfig& RuntimeScheduler::config() const noexcept {
    return config_;
}

SchedulerStats RuntimeScheduler::stats() const noexcept {
    return stats_;
}

std::size_t RuntimeScheduler::size() const noexcept {
    return tasks_.size();
}

bool RuntimeScheduler::empty() const noexcept {
    return tasks_.empty();
}

SchedulerSnapshot RuntimeScheduler::snapshot() const {
    SchedulerSnapshot snapshot;
    snapshot.stats = stats_;
    snapshot.tasks.reserve(tasks_.size());
    for (const auto& [unused, record] : tasks_) {
        (void)unused;
        if (record.state == SchedulerTaskState::queued) {
            ++snapshot.queued;
        }
        if (record.state == SchedulerTaskState::running) {
            ++snapshot.running;
        }
        snapshot.tasks.push_back(SchedulerSnapshotTask{
            record.task,
            record.state,
            record.attempts,
            record.last_lease_time_ns,
            record.notes,
        });
    }
    return snapshot;
}

SchedulerTaskId RuntimeScheduler::enqueue(SchedulerTask task) {
    if (tasks_.size() >= config_.max_tasks) {
        throw std::runtime_error("runtime scheduler capacity reached");
    }
    if (config_.reject_duplicate_labels && duplicate_label(task)) {
        ++stats_.duplicate_rejections;
        return 0;
    }
    if (task.id == 0) {
        task.id = next_id();
    } else {
        next_id_ = std::max(next_id_, task.id + 1);
    }
    TaskRecord record;
    record.task = std::move(task);
    note(record, "task enqueued");
    const auto id = record.task.id;
    tasks_[id] = std::move(record);
    ++stats_.enqueued;
    return id;
}

std::optional<SchedulerLease> RuntimeScheduler::lease_next(std::uint64_t now_ns) {
    auto iter = select(now_ns);
    if (iter == tasks_.end()) {
        return std::nullopt;
    }
    auto reason = pick_reason(iter->second, now_ns);
    record_lease(iter->second, now_ns);
    ++stats_.leased;
    return SchedulerLease{
        iter->second.task,
        reason,
        now_ns,
    };
}

std::vector<SchedulerLease> RuntimeScheduler::lease_many(std::uint64_t now_ns, std::size_t max_count) {
    std::vector<SchedulerLease> leases;
    leases.reserve(max_count);
    while (leases.size() < max_count) {
        auto lease = lease_next(now_ns);
        if (!lease) {
            break;
        }
        leases.push_back(std::move(*lease));
    }
    return leases;
}

bool RuntimeScheduler::complete(SchedulerCompletion completion) {
    auto found = tasks_.find(completion.id);
    if (found == tasks_.end()) {
        return false;
    }
    auto& record = found->second;
    if (record.state != SchedulerTaskState::running) {
        return false;
    }
    release_device(record.task.device);
    if (completion.success) {
        record.state = SchedulerTaskState::completed;
        note(record, completion.note.empty() ? "task completed" : std::move(completion.note));
        ++stats_.completed;
        return true;
    }
    record.state = SchedulerTaskState::queued;
    record.task.ready_time_ns = completion.retry_ready_time_ns;
    note(record, completion.note.empty() ? "task rescheduled after failure" : std::move(completion.note));
    ++stats_.retried;
    return true;
}

bool RuntimeScheduler::cancel(SchedulerTaskId id, std::string note_text) {
    auto found = tasks_.find(id);
    if (found == tasks_.end()) {
        return false;
    }
    auto& record = found->second;
    if (is_terminal(record.state)) {
        return false;
    }
    if (record.state == SchedulerTaskState::running) {
        release_device(record.task.device);
    }
    record.state = SchedulerTaskState::cancelled;
    note(record, std::move(note_text));
    ++stats_.cancelled;
    return true;
}

std::vector<SchedulerTask> RuntimeScheduler::expire(std::uint64_t now_ns) {
    std::vector<SchedulerTask> expired;
    for (auto& [unused, record] : tasks_) {
        (void)unused;
        if (record.state != SchedulerTaskState::queued) {
            continue;
        }
        if (record.task.deadline_ns != 0 && now_ns > record.task.deadline_ns) {
            record.state = SchedulerTaskState::expired;
            note(record, "task expired before lease");
            expired.push_back(record.task);
            ++stats_.expired;
        }
    }
    return expired;
}

void RuntimeScheduler::clear_completed() {
    for (auto iter = tasks_.begin(); iter != tasks_.end();) {
        if (is_terminal(iter->second.state)) {
            iter = tasks_.erase(iter);
        } else {
            ++iter;
        }
    }
}

void RuntimeScheduler::reset() {
    tasks_.clear();
    running_by_device_.clear();
    stats_ = {};
    next_id_ = 1;
}

SchedulerTaskId RuntimeScheduler::next_id() {
    return next_id_++;
}

bool RuntimeScheduler::duplicate_label(const SchedulerTask& task) const {
    if (task.label.empty()) {
        return false;
    }
    for (const auto& [unused, record] : tasks_) {
        (void)unused;
        if (is_terminal(record.state)) {
            continue;
        }
        if (record.task.device == task.device
            && record.task.kind == task.kind
            && record.task.label == task.label) {
            return true;
        }
    }
    return false;
}

std::map<SchedulerTaskId, RuntimeScheduler::TaskRecord>::iterator RuntimeScheduler::select(std::uint64_t now_ns) {
    auto best = tasks_.end();
    for (auto iter = tasks_.begin(); iter != tasks_.end(); ++iter) {
        auto& record = iter->second;
        if (record.state != SchedulerTaskState::queued) {
            continue;
        }
        if (record.task.ready_time_ns > now_ns) {
            continue;
        }
        if (!device_burst_available(record.task.device)) {
            continue;
        }
        if (best == tasks_.end()) {
            best = iter;
            continue;
        }

        const auto& lhs = record.task;
        const auto& rhs = best->second.task;
        const bool lhs_due = has_deadline(lhs) && lhs.deadline_ns <= now_ns + config_.fairness_window_ns;
        const bool rhs_due = has_deadline(rhs) && rhs.deadline_ns <= now_ns + config_.fairness_window_ns;
        if (lhs_due != rhs_due) {
            if (lhs_due) {
                best = iter;
            }
            continue;
        }
        if (has_deadline(lhs) && has_deadline(rhs) && lhs.deadline_ns != rhs.deadline_ns) {
            if (lhs.deadline_ns < rhs.deadline_ns) {
                best = iter;
            }
            continue;
        }
        if (priority_score(lhs.priority) != priority_score(rhs.priority)) {
            if (priority_score(lhs.priority) > priority_score(rhs.priority)) {
                best = iter;
            }
            continue;
        }
        if (record.attempts != best->second.attempts) {
            if (record.attempts < best->second.attempts) {
                best = iter;
            }
            continue;
        }
        if (lhs.enqueue_time_ns < rhs.enqueue_time_ns) {
            best = iter;
        }
    }
    return best;
}

SchedulerPickReason RuntimeScheduler::pick_reason(const TaskRecord& record, std::uint64_t now_ns) const {
    if (record.task.deadline_ns != 0 && record.task.deadline_ns <= now_ns + config_.fairness_window_ns) {
        return SchedulerPickReason::deadline;
    }
    if (priority_score(record.task.priority) >= priority_score(IngestPriority::high)) {
        return SchedulerPickReason::priority;
    }
    if (record.attempts > 0) {
        return SchedulerPickReason::fairness;
    }
    return SchedulerPickReason::ready;
}

bool RuntimeScheduler::device_burst_available(protocol::DeviceId device) const {
    auto found = running_by_device_.find(device);
    if (found == running_by_device_.end()) {
        return true;
    }
    return found->second < config_.per_device_burst;
}

void RuntimeScheduler::record_lease(TaskRecord& record, std::uint64_t now_ns) {
    record.state = SchedulerTaskState::running;
    record.last_lease_time_ns = now_ns;
    ++record.attempts;
    ++running_by_device_[record.task.device];
    note(record, "task leased");
}

void RuntimeScheduler::release_device(protocol::DeviceId device) {
    auto found = running_by_device_.find(device);
    if (found == running_by_device_.end()) {
        return;
    }
    if (found->second <= 1) {
        running_by_device_.erase(found);
    } else {
        --found->second;
    }
}

void RuntimeScheduler::note(TaskRecord& record, std::string message) {
    record.notes.push_back(std::move(message));
}

std::string scheduler_task_kind_name(SchedulerTaskKind kind) {
    switch (kind) {
    case SchedulerTaskKind::ingest:
        return "ingest";
    case SchedulerTaskKind::batch:
        return "batch";
    case SchedulerTaskKind::dispatch:
        return "dispatch";
    case SchedulerTaskKind::maintenance:
        return "maintenance";
    case SchedulerTaskKind::control:
        return "control";
    }
    return "unknown";
}

std::string scheduler_task_state_name(SchedulerTaskState state) {
    switch (state) {
    case SchedulerTaskState::queued:
        return "queued";
    case SchedulerTaskState::running:
        return "running";
    case SchedulerTaskState::completed:
        return "completed";
    case SchedulerTaskState::cancelled:
        return "cancelled";
    case SchedulerTaskState::expired:
        return "expired";
    }
    return "unknown";
}

std::string scheduler_pick_reason_name(SchedulerPickReason reason) {
    switch (reason) {
    case SchedulerPickReason::none:
        return "none";
    case SchedulerPickReason::ready:
        return "ready";
    case SchedulerPickReason::deadline:
        return "deadline";
    case SchedulerPickReason::priority:
        return "priority";
    case SchedulerPickReason::fairness:
        return "fairness";
    }
    return "unknown";
}

std::string render_scheduler_snapshot(const SchedulerSnapshot& snapshot) {
    std::ostringstream out;
    out << "runtime_scheduler\n"
        << "  queued: "
        << snapshot.queued
        << "\n"
        << "  running: "
        << snapshot.running
        << "\n"
        << "  enqueued: "
        << snapshot.stats.enqueued
        << "\n"
        << "  leased: "
        << snapshot.stats.leased
        << "\n"
        << "  completed: "
        << snapshot.stats.completed
        << "\n"
        << "  cancelled: "
        << snapshot.stats.cancelled
        << "\n"
        << "  expired: "
        << snapshot.stats.expired
        << "\n"
        << "  retried: "
        << snapshot.stats.retried
        << "\n";
    for (const auto& task : snapshot.tasks) {
        out << "  task: id="
            << task.task.id
            << " device="
            << task.task.device
            << " kind="
            << scheduler_task_kind_name(task.task.kind)
            << " state="
            << scheduler_task_state_name(task.state)
            << " attempts="
            << task.attempts
            << " label=\""
            << task.task.label
            << "\"\n";
    }
    return out.str();
}

} // namespace aethon::runtime
