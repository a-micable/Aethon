#include "aethon/replay/replay_engine.hpp"
#include "aethon/common/error.hpp"
#include <chrono>
#include <thread>
namespace aethon::replay {
ReplayEngine::ReplayEngine(ReplayOptions options) : options_(options) { if (options_.speed <= 0.0) throw Error(ErrorCode::invalid_argument, "replay speed must be positive"); }
bool ReplayEngine::within_window(std::uint64_t ts) const { return (options_.start_time_ns == 0 || ts >= options_.start_time_ns) && (options_.end_time_ns == 0 || ts <= options_.end_time_ns); }
ReplayStats ReplayEngine::run(storage::ArchiveReader& reader, Handler handler) { ReplayStats stats; std::uint64_t previous_ts = 0; while (auto record = reader.next()) { ++stats.records_seen; if (!within_window(record->capture_time_ns)) continue; if (options_.preserve_timing && previous_ts != 0 && record->capture_time_ns > previous_ts) { auto delta = static_cast<double>(record->capture_time_ns - previous_ts) / options_.speed; std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<std::uint64_t>(delta))); } previous_ts = record->capture_time_ns; handler(*record); ++stats.records_emitted; stats.bytes_emitted += record->packet.payload.size(); } return stats; }
} // namespace aethon::replay
