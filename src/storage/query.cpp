#include "aethon/storage/query.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

void count_time_skip(ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->skipped_by_time;
    }
}

void count_device_skip(ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->skipped_by_device;
    }
}

void count_kind_skip(ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->skipped_by_kind;
    }
}

void count_payload_skip(ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->skipped_by_payload;
    }
}

bool time_matches(std::uint64_t capture_time_ns, const ArchiveQuery& query, ArchiveQueryStats* stats) {
    if (query.start_time_ns && capture_time_ns < *query.start_time_ns) {
        count_time_skip(stats);
        return false;
    }
    if (query.end_time_ns && capture_time_ns > *query.end_time_ns) {
        count_time_skip(stats);
        return false;
    }
    return true;
}

bool payload_matches(std::size_t payload_size, const ArchiveQuery& query, ArchiveQueryStats* stats) {
    if (query.min_payload_size && payload_size < *query.min_payload_size) {
        count_payload_skip(stats);
        return false;
    }
    if (query.max_payload_size && payload_size > *query.max_payload_size) {
        count_payload_skip(stats);
        return false;
    }
    return true;
}

DeviceArchiveSummary* find_device_summary(std::vector<DeviceArchiveSummary>& summaries,
                                          protocol::DeviceId device) {
    auto it = std::find_if(
        summaries.begin(),
        summaries.end(),
        [device](const DeviceArchiveSummary& summary) {
            return summary.device == device;
        });
    if (it == summaries.end()) {
        summaries.push_back(DeviceArchiveSummary{device});
        return &summaries.back();
    }
    return &*it;
}

void update_device_summary(DeviceArchiveSummary& summary, const ArchiveRecord& record) {
    if (summary.record_count == 0) {
        summary.first_time_ns = record.capture_time_ns;
        summary.last_time_ns = record.capture_time_ns;
    } else {
        summary.first_time_ns = std::min(summary.first_time_ns, record.capture_time_ns);
        summary.last_time_ns = std::max(summary.last_time_ns, record.capture_time_ns);
    }
    ++summary.record_count;
    summary.total_payload_bytes += record.packet.payload.size();
}

} // namespace

bool matches_query(const ArchiveRecord& record, const ArchiveQuery& query, ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->scanned_records;
    }
    if (!time_matches(record.capture_time_ns, query, stats)) {
        return false;
    }
    if (query.device && record.packet.device != *query.device) {
        count_device_skip(stats);
        return false;
    }
    if (query.kind && record.packet.kind != *query.kind) {
        count_kind_skip(stats);
        return false;
    }
    if (!payload_matches(record.packet.payload.size(), query, stats)) {
        return false;
    }
    if (stats != nullptr) {
        ++stats->matched_records;
    }
    return true;
}

ArchiveQueryResult query_archive(const std::filesystem::path& path, const ArchiveQuery& query) {
    ArchiveReader reader(path);
    ArchiveQueryResult result;
    while (auto record = reader.next()) {
        if (!matches_query(*record, query, &result.stats)) {
            continue;
        }
        result.records.push_back(std::move(*record));
        if (query.limit != 0 && result.records.size() >= query.limit) {
            break;
        }
    }
    return result;
}

std::vector<DeviceArchiveSummary> summarize_by_device(const std::filesystem::path& path) {
    ArchiveReader reader(path);
    std::vector<DeviceArchiveSummary> summaries;
    while (auto record = reader.next()) {
        auto* summary = find_device_summary(summaries, record->packet.device);
        update_device_summary(*summary, *record);
    }
    std::sort(
        summaries.begin(),
        summaries.end(),
        [](const DeviceArchiveSummary& left, const DeviceArchiveSummary& right) {
            if (left.record_count != right.record_count) {
                return left.record_count > right.record_count;
            }
            return left.device < right.device;
        });
    return summaries;
}

std::vector<ArchiveIndexEntry> filter_index(const ArchiveIndex& index, const ArchiveQuery& query) {
    std::vector<ArchiveIndexEntry> entries;
    for (const auto& entry : index.records) {
        if (!time_matches(entry.capture_time_ns, query, nullptr)) {
            continue;
        }
        if (query.device && entry.device != *query.device) {
            continue;
        }
        if (!payload_matches(static_cast<std::size_t>(entry.payload_size), query, nullptr)) {
            continue;
        }
        entries.push_back(entry);
        if (query.limit != 0 && entries.size() >= query.limit) {
            break;
        }
    }
    return entries;
}

std::string describe_query(const ArchiveQuery& query) {
    std::ostringstream out;
    out << "archive_query";
    if (query.start_time_ns) {
        out << " start_time_ns=" << *query.start_time_ns;
    }
    if (query.end_time_ns) {
        out << " end_time_ns=" << *query.end_time_ns;
    }
    if (query.device) {
        out << " device=" << *query.device;
    }
    if (query.kind) {
        out << " kind=" << static_cast<unsigned>(*query.kind);
    }
    if (query.min_payload_size) {
        out << " min_payload_size=" << *query.min_payload_size;
    }
    if (query.max_payload_size) {
        out << " max_payload_size=" << *query.max_payload_size;
    }
    if (query.limit != 0) {
        out << " limit=" << query.limit;
    }
    return out.str();
}

QuerySelectivity calculate_selectivity(const ArchiveQueryStats& stats) {
    QuerySelectivity selectivity;
    if (stats.scanned_records == 0) {
        return selectivity;
    }
    auto denominator = static_cast<double>(stats.scanned_records);
    selectivity.match_ratio = static_cast<double>(stats.matched_records) / denominator;
    selectivity.time_reject_ratio = static_cast<double>(stats.skipped_by_time) / denominator;
    selectivity.device_reject_ratio = static_cast<double>(stats.skipped_by_device) / denominator;
    selectivity.kind_reject_ratio = static_cast<double>(stats.skipped_by_kind) / denominator;
    selectivity.payload_reject_ratio = static_cast<double>(stats.skipped_by_payload) / denominator;
    return selectivity;
}

std::string render_query_stats(const ArchiveQueryStats& stats) {
    std::ostringstream out;
    out << "query_stats\n"
        << "  scanned_records: " << stats.scanned_records << "\n"
        << "  matched_records: " << stats.matched_records << "\n"
        << "  skipped_by_time: " << stats.skipped_by_time << "\n"
        << "  skipped_by_device: " << stats.skipped_by_device << "\n"
        << "  skipped_by_kind: " << stats.skipped_by_kind << "\n"
        << "  skipped_by_payload: " << stats.skipped_by_payload << "\n";
    return out.str();
}

std::string render_query_selectivity(const QuerySelectivity& selectivity) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << "query_selectivity\n"
        << "  match_ratio: " << selectivity.match_ratio << "\n"
        << "  time_reject_ratio: " << selectivity.time_reject_ratio << "\n"
        << "  device_reject_ratio: " << selectivity.device_reject_ratio << "\n"
        << "  kind_reject_ratio: " << selectivity.kind_reject_ratio << "\n"
        << "  payload_reject_ratio: " << selectivity.payload_reject_ratio << "\n";
    return out.str();
}

std::string render_device_summaries(const std::vector<DeviceArchiveSummary>& summaries) {
    std::ostringstream out;
    out << "device_summaries\n";
    for (const auto& summary : summaries) {
        out << "  device: " << summary.device
            << " records=" << summary.record_count
            << " first_time_ns=" << summary.first_time_ns
            << " last_time_ns=" << summary.last_time_ns
            << " payload_bytes=" << summary.total_payload_bytes << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
