#include "aethon/runtime/collector.hpp"

#include <sstream>
#include <utility>

namespace aethon::runtime {

CollectorRuntime::CollectorRuntime(config::CollectorProfile profile)
    : profile_(std::move(profile)) {
    open_archive_if_needed();
}

CollectorRuntime::~CollectorRuntime() = default;

void CollectorRuntime::subscribe(RuntimeEventHandler handler) {
    dispatcher_.subscribe(std::move(handler));
}

CollectorResult CollectorRuntime::accept(protocol::Packet packet, std::uint64_t arrival_time_ns) {
    CollectorResult result;
    ++stats_.packets_received;

    auto validation = protocol::validate_packet_semantics(packet);
    if (!validation.ok()) {
        ++stats_.packets_rejected;
        result.accepted = false;
        result.messages.push_back(protocol::render_packet_validation_report(validation));
        publish(RuntimeEventKind::error, arrival_time_ns, packet, "packet rejected by semantic validation");
        return result;
    }

    for (const auto& issue : validation.issues) {
        if (issue.severity == protocol::ValidationSeverity::warning) {
            ++stats_.warnings_emitted;
            result.messages.push_back(issue.message);
            publish(RuntimeEventKind::warning, arrival_time_ns, packet, issue.message);
        }
    }

    links_.observe(packet, arrival_time_ns);
    auto session_events = sessions_.observe(packet, arrival_time_ns);
    for (const auto& event : session_events) {
        publish(RuntimeEventKind::session_event, arrival_time_ns, packet, stream::render_session_event(event));
    }

    if (archive_) {
        archive_->append(arrival_time_ns, packet);
        ++stats_.packets_archived;
        publish(RuntimeEventKind::packet_archived, arrival_time_ns, packet, "packet archived");
    }

    result.accepted = true;
    publish(RuntimeEventKind::packet_received, arrival_time_ns, packet, "packet accepted");
    return result;
}

CollectorStats CollectorRuntime::stats() const noexcept {
    return stats_;
}

stream::SessionTrackerSnapshot CollectorRuntime::sessions() const {
    return sessions_.snapshot();
}

stream::LinkMonitorReport CollectorRuntime::link_report() const {
    return links_.report();
}

void CollectorRuntime::open_archive_if_needed() {
    if (!profile_.archive.enabled) {
        return;
    }
    archive_ = std::make_unique<storage::ArchiveWriter>(profile_.archive.path);
}

void CollectorRuntime::publish(RuntimeEventKind kind,
                               std::uint64_t time_ns,
                               const protocol::Packet& packet,
                               std::string message) {
    RuntimeEvent event;
    event.kind = kind;
    event.time_ns = time_ns;
    event.packet = packet;
    event.message = std::move(message);
    dispatcher_.publish(event);
}

std::string render_collector_stats(const CollectorStats& stats) {
    std::ostringstream out;
    out << "collector_stats\n"
        << "  packets_received: "
        << stats.packets_received
        << "\n"
        << "  packets_archived: "
        << stats.packets_archived
        << "\n"
        << "  packets_rejected: "
        << stats.packets_rejected
        << "\n"
        << "  warnings_emitted: "
        << stats.warnings_emitted
        << "\n";
    return out.str();
}

std::string render_collector_result(const CollectorResult& result) {
    std::ostringstream out;
    out << "collector_result\n"
        << "  accepted: "
        << (result.accepted ? "yes" : "no")
        << "\n";
    for (const auto& message : result.messages) {
        out << "  message: "
            << message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::runtime
