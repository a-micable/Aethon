#include "aethon/runtime/replay_driver.hpp"

#include "aethon/storage/archive.hpp"

#include <sstream>
#include <utility>

namespace aethon::runtime {

ReplayDriver::ReplayDriver(replay::ReplayOptions options)
    : options_(options) {}

void ReplayDriver::subscribe(RuntimeEventHandler handler) {
    dispatcher_.subscribe(std::move(handler));
}

ReplayDriverResult ReplayDriver::run(const std::filesystem::path& archive_path) {
    storage::ArchiveReader reader(archive_path);
    replay::ReplayEngine engine(options_);
    ReplayDriverResult result;
    result.stats = engine.run(reader, [&](const storage::ArchiveRecord& record) {
        RuntimeEvent event;
        event.kind = RuntimeEventKind::packet_received;
        event.time_ns = record.capture_time_ns;
        event.packet = record.packet;
        event.message = "replayed archive packet";
        dispatcher_.publish(event);
        ++result.events_published;
    });
    return result;
}

std::string render_replay_driver_result(const ReplayDriverResult& result) {
    std::ostringstream out;
    out << "replay_driver_result\n"
        << "  records_seen: "
        << result.stats.records_seen
        << "\n"
        << "  records_emitted: "
        << result.stats.records_emitted
        << "\n"
        << "  events_published: "
        << result.events_published
        << "\n";
    return out.str();
}

} // namespace aethon::runtime
