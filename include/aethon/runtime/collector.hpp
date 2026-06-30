#pragma once

#include "aethon/config/profile.hpp"
#include "aethon/protocol/validator.hpp"
#include "aethon/runtime/event_dispatcher.hpp"
#include "aethon/storage/archive.hpp"
#include "aethon/stream/link_monitor.hpp"
#include "aethon/stream/session_tracker.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aethon::runtime {

struct CollectorStats {
    std::uint64_t packets_received = 0;
    std::uint64_t packets_archived = 0;
    std::uint64_t packets_rejected = 0;
    std::uint64_t warnings_emitted = 0;
};

struct CollectorResult {
    bool accepted = false;
    std::vector<std::string> messages;
};

class CollectorRuntime {
public:
    explicit CollectorRuntime(config::CollectorProfile profile);
    ~CollectorRuntime();

    CollectorRuntime(const CollectorRuntime&) = delete;
    CollectorRuntime& operator=(const CollectorRuntime&) = delete;

    void subscribe(RuntimeEventHandler handler);
    [[nodiscard]] CollectorResult accept(protocol::Packet packet, std::uint64_t arrival_time_ns);
    [[nodiscard]] CollectorStats stats() const noexcept;
    [[nodiscard]] stream::SessionTrackerSnapshot sessions() const;
    [[nodiscard]] stream::LinkMonitorReport link_report() const;

private:
    void open_archive_if_needed();
    void publish(RuntimeEventKind kind,
                 std::uint64_t time_ns,
                 const protocol::Packet& packet,
                 std::string message);

    config::CollectorProfile profile_;
    CollectorStats stats_;
    EventDispatcher dispatcher_;
    stream::SessionTracker sessions_;
    stream::LinkMonitor links_;
    std::unique_ptr<storage::ArchiveWriter> archive_;
};

[[nodiscard]] std::string render_collector_stats(const CollectorStats& stats);
[[nodiscard]] std::string render_collector_result(const CollectorResult& result);

} // namespace aethon::runtime
