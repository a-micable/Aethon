#include "aethon/storage/compaction.hpp"

#include "aethon/storage/archive.hpp"

#include <map>
#include <sstream>

namespace aethon::storage {
namespace {

std::map<protocol::DeviceId, std::uint32_t> latest_sequences(const std::filesystem::path& source) {
    ArchiveReader reader(source);
    std::map<protocol::DeviceId, std::uint32_t> latest;
    while (auto record = reader.next()) {
        auto& current = latest[record->packet.device];
        if (record->packet.sequence > current) {
            current = record->packet.sequence;
        }
    }
    return latest;
}

bool is_superseded(const ArchiveRecord& record,
                   const std::map<protocol::DeviceId, std::uint32_t>& latest) {
    auto it = latest.find(record.packet.device);
    if (it == latest.end()) {
        return false;
    }
    return record.packet.sequence < it->second;
}

} // namespace

CompactionPlan plan_compaction(const std::filesystem::path& source,
                               const std::filesystem::path& destination,
                               const CompactionOptions& options) {
    CompactionPlan plan;
    plan.source = source;
    plan.destination = destination;
    plan.options = options;
    plan.stats = compact_archive(source, destination, options);
    if (plan.stats.records_written == 0) {
        plan.notes.push_back("compaction wrote no records");
    }
    if (plan.stats.records_dropped_superseded != 0) {
        plan.notes.push_back("older per-device records were superseded");
    }
    return plan;
}

CompactionStats compact_archive(const std::filesystem::path& source,
                                const std::filesystem::path& destination,
                                const CompactionOptions& options) {
    auto latest = options.keep_latest_per_device
        ? latest_sequences(source)
        : std::map<protocol::DeviceId, std::uint32_t>{};

    ArchiveReader reader(source);
    ArchiveWriter writer(destination);
    CompactionStats stats;

    while (auto record = reader.next()) {
        ++stats.records_read;
        if (!matches_query(*record, options.query)) {
            ++stats.records_dropped_by_query;
            continue;
        }
        if (options.drop_empty_payloads && record->packet.payload.empty()) {
            ++stats.records_dropped_empty;
            continue;
        }
        if (options.keep_latest_per_device && is_superseded(*record, latest)) {
            ++stats.records_dropped_superseded;
            continue;
        }
        writer.append(record->capture_time_ns, record->packet);
        ++stats.records_written;
    }
    writer.close();
    return stats;
}

std::string render_compaction_plan(const CompactionPlan& plan) {
    std::ostringstream out;
    out << "compaction_plan\n"
        << "  source: "
        << plan.source.string()
        << "\n"
        << "  destination: "
        << plan.destination.string()
        << "\n"
        << render_compaction_stats(plan.stats);
    for (const auto& note : plan.notes) {
        out << "  note: "
            << note
            << "\n";
    }
    return out.str();
}

std::string render_compaction_stats(const CompactionStats& stats) {
    std::ostringstream out;
    out << "  records_read: "
        << stats.records_read
        << "\n"
        << "  records_written: "
        << stats.records_written
        << "\n"
        << "  records_dropped_by_query: "
        << stats.records_dropped_by_query
        << "\n"
        << "  records_dropped_empty: "
        << stats.records_dropped_empty
        << "\n"
        << "  records_dropped_superseded: "
        << stats.records_dropped_superseded
        << "\n";
    return out.str();
}

} // namespace aethon::storage
