#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/storage/archive.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

struct ArchiveQuery {
    std::optional<std::uint64_t> start_time_ns;
    std::optional<std::uint64_t> end_time_ns;
    std::optional<protocol::DeviceId> device;
    std::optional<protocol::PacketKind> kind;
    std::optional<std::size_t> min_payload_size;
    std::optional<std::size_t> max_payload_size;
    std::size_t limit = 0;
};

struct ArchiveQueryStats {
    std::uint64_t scanned_records = 0;
    std::uint64_t matched_records = 0;
    std::uint64_t skipped_by_time = 0;
    std::uint64_t skipped_by_device = 0;
    std::uint64_t skipped_by_kind = 0;
    std::uint64_t skipped_by_payload = 0;
};

struct ArchiveQueryResult {
    std::vector<ArchiveRecord> records;
    ArchiveQueryStats stats;
};

struct QuerySelectivity {
    double match_ratio = 0.0;
    double time_reject_ratio = 0.0;
    double device_reject_ratio = 0.0;
    double kind_reject_ratio = 0.0;
    double payload_reject_ratio = 0.0;
};

struct DeviceArchiveSummary {
    protocol::DeviceId device = 0;
    std::uint64_t record_count = 0;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
    std::uint64_t total_payload_bytes = 0;
};

[[nodiscard]] bool matches_query(const ArchiveRecord& record, const ArchiveQuery& query,
                                 ArchiveQueryStats* stats = nullptr);

[[nodiscard]] ArchiveQueryResult query_archive(const std::filesystem::path& path,
                                               const ArchiveQuery& query);

[[nodiscard]] std::vector<DeviceArchiveSummary> summarize_by_device(const std::filesystem::path& path);

[[nodiscard]] std::vector<ArchiveIndexEntry> filter_index(const ArchiveIndex& index,
                                                          const ArchiveQuery& query);

[[nodiscard]] std::string describe_query(const ArchiveQuery& query);
[[nodiscard]] QuerySelectivity calculate_selectivity(const ArchiveQueryStats& stats);
[[nodiscard]] std::string render_query_stats(const ArchiveQueryStats& stats);
[[nodiscard]] std::string render_query_selectivity(const QuerySelectivity& selectivity);
[[nodiscard]] std::string render_device_summaries(const std::vector<DeviceArchiveSummary>& summaries);

} // namespace aethon::storage
