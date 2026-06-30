#pragma once

#include "aethon/storage/archive_collection.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

struct ArchiveTimeBucket {
    std::uint64_t bucket_start_ns = 0;
    std::uint64_t bucket_end_ns = 0;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t archive_count = 0;
};

struct DeviceTimelinePoint {
    protocol::DeviceId device = 0;
    std::uint64_t capture_time_ns = 0;
    std::uint32_t sequence = 0;
    std::uint64_t payload_size = 0;
    std::filesystem::path archive_path;
};

struct DeviceTimeline {
    protocol::DeviceId device = 0;
    std::vector<DeviceTimelinePoint> points;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
    std::uint64_t total_payload_bytes = 0;
};

struct ArchiveGap {
    protocol::DeviceId device = 0;
    std::uint64_t previous_time_ns = 0;
    std::uint64_t next_time_ns = 0;
    std::uint64_t gap_ns = 0;
    std::filesystem::path previous_archive;
    std::filesystem::path next_archive;
};

struct ArchiveOverlap {
    std::filesystem::path left_archive;
    std::filesystem::path right_archive;
    std::uint64_t overlap_start_ns = 0;
    std::uint64_t overlap_end_ns = 0;
    std::uint64_t overlap_span_ns = 0;
    std::uint64_t left_records_in_overlap = 0;
    std::uint64_t right_records_in_overlap = 0;
};

struct SequenceAnomaly {
    protocol::DeviceId device = 0;
    std::uint32_t previous_sequence = 0;
    std::uint32_t current_sequence = 0;
    std::uint64_t capture_time_ns = 0;
    std::filesystem::path archive_path;
    std::string reason;
};

struct PayloadPercentiles {
    std::uint64_t minimum = 0;
    std::uint64_t p50 = 0;
    std::uint64_t p90 = 0;
    std::uint64_t p99 = 0;
    std::uint64_t maximum = 0;
    double average = 0.0;
};

struct DevicePayloadRank {
    protocol::DeviceId device = 0;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    double payload_share = 0.0;
    double average_payload_size = 0.0;
};

struct ArchiveDensitySummary {
    std::uint64_t archive_count = 0;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    double records_per_archive = 0.0;
    double payload_bytes_per_archive = 0.0;
    double payload_bytes_per_record = 0.0;
};

struct DeviceKindMatrixCell {
    protocol::DeviceId device = 0;
    protocol::PacketKind kind = protocol::PacketKind::heartbeat;
    std::uint64_t record_count = 0;
};

struct ArchiveStatisticsReport {
    ArchiveSummary summary;
    std::vector<ArchiveTimeBucket> time_buckets;
    std::vector<DeviceTimeline> device_timelines;
    std::vector<ArchiveGap> gaps;
    std::vector<ArchiveOverlap> overlaps;
    std::vector<SequenceAnomaly> sequence_anomalies;
    std::vector<DevicePayloadRank> payload_ranking;
    std::vector<DeviceKindMatrixCell> device_kind_matrix;
    PayloadPercentiles payload_percentiles;
    ArchiveDensitySummary density;
};

[[nodiscard]] std::vector<ArchiveTimeBucket> build_archive_time_buckets(const ArchiveCollection& collection,
                                                                        std::uint64_t bucket_width_ns);
[[nodiscard]] std::vector<DeviceTimeline> build_device_timelines(const ArchiveCollection& collection);
[[nodiscard]] std::vector<ArchiveGap> detect_archive_gaps(const ArchiveCollection& collection,
                                                          std::uint64_t gap_threshold_ns);
[[nodiscard]] std::vector<ArchiveOverlap> detect_archive_overlaps(const ArchiveCollection& collection);
[[nodiscard]] std::vector<SequenceAnomaly> detect_sequence_anomalies(const ArchiveCollection& collection);
[[nodiscard]] PayloadPercentiles calculate_payload_percentiles(const ArchiveCollection& collection);
[[nodiscard]] std::vector<DevicePayloadRank> rank_devices_by_payload(const ArchiveCollection& collection);
[[nodiscard]] ArchiveDensitySummary calculate_archive_density(const ArchiveCollection& collection);
[[nodiscard]] bool archive_density_empty(const ArchiveDensitySummary& density);
[[nodiscard]] bool archive_statistics_report_empty(const ArchiveStatisticsReport& report);
[[nodiscard]] bool archive_statistics_report_has_findings(const ArchiveStatisticsReport& report);
[[nodiscard]] std::vector<DeviceKindMatrixCell> build_device_kind_matrix(const ArchiveCollection& collection);
[[nodiscard]] ArchiveStatisticsReport build_archive_statistics_report(const ArchiveCollection& collection,
                                                                      std::uint64_t bucket_width_ns,
                                                                      std::uint64_t gap_threshold_ns);
[[nodiscard]] std::string render_archive_time_buckets(const std::vector<ArchiveTimeBucket>& buckets);
[[nodiscard]] std::string render_device_timelines(const std::vector<DeviceTimeline>& timelines);
[[nodiscard]] std::string render_archive_gaps(const std::vector<ArchiveGap>& gaps);
[[nodiscard]] std::string render_archive_overlaps(const std::vector<ArchiveOverlap>& overlaps);
[[nodiscard]] std::string render_sequence_anomalies(const std::vector<SequenceAnomaly>& anomalies);
[[nodiscard]] std::string render_payload_percentiles(const PayloadPercentiles& percentiles);
[[nodiscard]] std::string render_device_payload_ranking(const std::vector<DevicePayloadRank>& ranking);
[[nodiscard]] std::string render_archive_density(const ArchiveDensitySummary& density);
[[nodiscard]] std::string render_device_kind_matrix(const std::vector<DeviceKindMatrixCell>& matrix);
[[nodiscard]] std::string render_archive_statistics_report(const ArchiveStatisticsReport& report);

} // namespace aethon::storage
