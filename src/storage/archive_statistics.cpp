#include "aethon/storage/archive_statistics.hpp"

#include "aethon/protocol/inspector.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

namespace aethon::storage {
namespace {

std::uint64_t bucket_start(std::uint64_t capture_time_ns, std::uint64_t width) {
    if (width == 0) {
        return capture_time_ns;
    }
    return (capture_time_ns / width) * width;
}

std::uint64_t percentile_value(const std::vector<std::uint64_t>& sorted, double percentile) {
    if (sorted.empty()) {
        return 0;
    }
    auto rank = percentile * static_cast<double>(sorted.size() - 1);
    auto index = static_cast<std::size_t>(rank + 0.5);
    if (index >= sorted.size()) {
        index = sorted.size() - 1;
    }
    return sorted[index];
}

DeviceTimelinePoint make_timeline_point(const ArchiveCollectionRecordRef& ref) {
    DeviceTimelinePoint point;
    point.device = ref.device;
    point.capture_time_ns = ref.capture_time_ns;
    point.sequence = ref.sequence;
    point.payload_size = ref.payload_size;
    point.archive_path = ref.path;
    return point;
}

void update_timeline(DeviceTimeline& timeline, const DeviceTimelinePoint& point) {
    if (timeline.points.empty()) {
        timeline.first_time_ns = point.capture_time_ns;
        timeline.last_time_ns = point.capture_time_ns;
    } else {
        timeline.first_time_ns = std::min(timeline.first_time_ns, point.capture_time_ns);
        timeline.last_time_ns = std::max(timeline.last_time_ns, point.capture_time_ns);
    }
    timeline.total_payload_bytes += point.payload_size;
    timeline.points.push_back(point);
}

std::map<protocol::DeviceId, DeviceTimeline> timeline_map(const ArchiveCollection& collection) {
    std::map<protocol::DeviceId, DeviceTimeline> timelines;
    for (const auto& ref : collection.records) {
        auto& timeline = timelines[ref.device];
        timeline.device = ref.device;
        update_timeline(timeline, make_timeline_point(ref));
    }
    for (auto& [device, timeline] : timelines) {
        (void)device;
        std::stable_sort(
            timeline.points.begin(),
            timeline.points.end(),
            [](const DeviceTimelinePoint& left, const DeviceTimelinePoint& right) {
                if (left.capture_time_ns != right.capture_time_ns) {
                    return left.capture_time_ns < right.capture_time_ns;
                }
                return left.sequence < right.sequence;
            });
    }
    return timelines;
}

std::uint64_t count_records_in_window(const ArchiveCollectionEntry& archive,
                                      std::uint64_t start_ns,
                                      std::uint64_t end_ns) {
    std::uint64_t count = 0;
    for (const auto& record : archive.index.records) {
        if (record.capture_time_ns >= start_ns && record.capture_time_ns <= end_ns) {
            ++count;
        }
    }
    return count;
}

bool archives_have_time_overlap(const ArchiveCollectionEntry& left,
                                const ArchiveCollectionEntry& right) {
    if (left.manifest.summary.record_count == 0 || right.manifest.summary.record_count == 0) {
        return false;
    }
    return left.manifest.summary.first_time_ns <= right.manifest.summary.last_time_ns
        && right.manifest.summary.first_time_ns <= left.manifest.summary.last_time_ns;
}

ArchiveOverlap make_overlap(const ArchiveCollectionEntry& left,
                            const ArchiveCollectionEntry& right) {
    ArchiveOverlap overlap;
    overlap.left_archive = left.path;
    overlap.right_archive = right.path;
    overlap.overlap_start_ns = std::max(left.manifest.summary.first_time_ns,
                                        right.manifest.summary.first_time_ns);
    overlap.overlap_end_ns = std::min(left.manifest.summary.last_time_ns,
                                      right.manifest.summary.last_time_ns);
    if (overlap.overlap_end_ns >= overlap.overlap_start_ns) {
        overlap.overlap_span_ns = overlap.overlap_end_ns - overlap.overlap_start_ns;
    }
    overlap.left_records_in_overlap = count_records_in_window(
        left,
        overlap.overlap_start_ns,
        overlap.overlap_end_ns);
    overlap.right_records_in_overlap = count_records_in_window(
        right,
        overlap.overlap_start_ns,
        overlap.overlap_end_ns);
    return overlap;
}

std::string anomaly_reason(std::uint32_t previous, std::uint32_t current) {
    if (current == previous) {
        return "duplicate sequence";
    }
    if (current < previous) {
        return "sequence moved backwards";
    }
    return "sequence gap";
}

ArchiveSummary summarize_collection(const ArchiveCollection& collection) {
    ArchiveSummary summary;
    summary.format_version = collection.summary.format_version;
    summary.record_count = collection.records.size();
    if (!collection.records.empty()) {
        summary.first_time_ns = collection.records.front().capture_time_ns;
        summary.last_time_ns = collection.records.front().capture_time_ns;
        for (const auto& ref : collection.records) {
            summary.first_time_ns = std::min(summary.first_time_ns, ref.capture_time_ns);
            summary.last_time_ns = std::max(summary.last_time_ns, ref.capture_time_ns);
        }
    }
    return summary;
}

} // namespace

std::vector<ArchiveTimeBucket> build_archive_time_buckets(const ArchiveCollection& collection,
                                                         std::uint64_t bucket_width_ns) {
    std::map<std::uint64_t, ArchiveTimeBucket> buckets;
    auto width = bucket_width_ns == 0 ? 1 : bucket_width_ns;
    for (const auto& ref : collection.records) {
        auto start = bucket_start(ref.capture_time_ns, width);
        auto& bucket = buckets[start];
        if (bucket.record_count == 0) {
            bucket.bucket_start_ns = start;
            bucket.bucket_end_ns = start + width - 1;
        }
        ++bucket.record_count;
        bucket.payload_bytes += ref.payload_size;
    }
    for (auto& [start, bucket] : buckets) {
        (void)start;
        std::set<std::uint64_t> archives;
        for (const auto& ref : collection.records) {
            if (ref.capture_time_ns >= bucket.bucket_start_ns
                && ref.capture_time_ns <= bucket.bucket_end_ns) {
                archives.insert(ref.archive_ordinal);
            }
        }
        bucket.archive_count = archives.size();
    }
    std::vector<ArchiveTimeBucket> out;
    out.reserve(buckets.size());
    for (const auto& [start, bucket] : buckets) {
        (void)start;
        out.push_back(bucket);
    }
    return out;
}

std::vector<DeviceTimeline> build_device_timelines(const ArchiveCollection& collection) {
    auto mapped = timeline_map(collection);
    std::vector<DeviceTimeline> timelines;
    timelines.reserve(mapped.size());
    for (auto& [device, timeline] : mapped) {
        (void)device;
        timelines.push_back(std::move(timeline));
    }
    std::sort(
        timelines.begin(),
        timelines.end(),
        [](const DeviceTimeline& left, const DeviceTimeline& right) {
            if (left.points.size() != right.points.size()) {
                return left.points.size() > right.points.size();
            }
            return left.device < right.device;
        });
    return timelines;
}

std::vector<ArchiveGap> detect_archive_gaps(const ArchiveCollection& collection,
                                            std::uint64_t gap_threshold_ns) {
    std::vector<ArchiveGap> gaps;
    auto timelines = build_device_timelines(collection);
    for (const auto& timeline : timelines) {
        if (timeline.points.size() < 2) {
            continue;
        }
        for (std::size_t i = 1; i < timeline.points.size(); ++i) {
            const auto& previous = timeline.points[i - 1];
            const auto& current = timeline.points[i];
            if (current.capture_time_ns < previous.capture_time_ns) {
                continue;
            }
            auto gap = current.capture_time_ns - previous.capture_time_ns;
            if (gap <= gap_threshold_ns) {
                continue;
            }
            ArchiveGap item;
            item.device = timeline.device;
            item.previous_time_ns = previous.capture_time_ns;
            item.next_time_ns = current.capture_time_ns;
            item.gap_ns = gap;
            item.previous_archive = previous.archive_path;
            item.next_archive = current.archive_path;
            gaps.push_back(std::move(item));
        }
    }
    std::sort(
        gaps.begin(),
        gaps.end(),
        [](const ArchiveGap& left, const ArchiveGap& right) {
            if (left.gap_ns != right.gap_ns) {
                return left.gap_ns > right.gap_ns;
            }
            return left.device < right.device;
        });
    return gaps;
}

std::vector<ArchiveOverlap> detect_archive_overlaps(const ArchiveCollection& collection) {
    std::vector<ArchiveOverlap> overlaps;
    if (collection.archives.size() < 2) {
        return overlaps;
    }
    for (std::size_t i = 0; i < collection.archives.size(); ++i) {
        for (std::size_t j = i + 1; j < collection.archives.size(); ++j) {
            const auto& left = collection.archives[i];
            const auto& right = collection.archives[j];
            if (!archives_have_time_overlap(left, right)) {
                continue;
            }
            overlaps.push_back(make_overlap(left, right));
        }
    }
    std::sort(
        overlaps.begin(),
        overlaps.end(),
        [](const ArchiveOverlap& left, const ArchiveOverlap& right) {
            if (left.overlap_span_ns != right.overlap_span_ns) {
                return left.overlap_span_ns > right.overlap_span_ns;
            }
            if (left.left_archive != right.left_archive) {
                return left.left_archive.string() < right.left_archive.string();
            }
            return left.right_archive.string() < right.right_archive.string();
        });
    return overlaps;
}

std::vector<SequenceAnomaly> detect_sequence_anomalies(const ArchiveCollection& collection) {
    std::vector<SequenceAnomaly> anomalies;
    auto timelines = build_device_timelines(collection);
    for (const auto& timeline : timelines) {
        if (timeline.points.size() < 2) {
            continue;
        }
        for (std::size_t i = 1; i < timeline.points.size(); ++i) {
            const auto& previous = timeline.points[i - 1];
            const auto& current = timeline.points[i];
            bool duplicate = current.sequence == previous.sequence;
            bool backwards = current.sequence < previous.sequence;
            bool gap = current.sequence > previous.sequence + 1;
            if (!duplicate && !backwards && !gap) {
                continue;
            }
            SequenceAnomaly anomaly;
            anomaly.device = timeline.device;
            anomaly.previous_sequence = previous.sequence;
            anomaly.current_sequence = current.sequence;
            anomaly.capture_time_ns = current.capture_time_ns;
            anomaly.archive_path = current.archive_path;
            anomaly.reason = anomaly_reason(previous.sequence, current.sequence);
            anomalies.push_back(std::move(anomaly));
        }
    }
    std::sort(
        anomalies.begin(),
        anomalies.end(),
        [](const SequenceAnomaly& left, const SequenceAnomaly& right) {
            if (left.device != right.device) {
                return left.device < right.device;
            }
            return left.capture_time_ns < right.capture_time_ns;
        });
    return anomalies;
}

PayloadPercentiles calculate_payload_percentiles(const ArchiveCollection& collection) {
    PayloadPercentiles percentiles;
    if (collection.records.empty()) {
        return percentiles;
    }
    std::vector<std::uint64_t> sizes;
    sizes.reserve(collection.records.size());
    for (const auto& ref : collection.records) {
        sizes.push_back(ref.payload_size);
    }
    std::sort(sizes.begin(), sizes.end());
    auto total = std::accumulate(sizes.begin(), sizes.end(), std::uint64_t{0});
    percentiles.minimum = sizes.front();
    percentiles.p50 = percentile_value(sizes, 0.50);
    percentiles.p90 = percentile_value(sizes, 0.90);
    percentiles.p99 = percentile_value(sizes, 0.99);
    percentiles.maximum = sizes.back();
    percentiles.average = static_cast<double>(total) / static_cast<double>(sizes.size());
    return percentiles;
}

std::vector<DevicePayloadRank> rank_devices_by_payload(const ArchiveCollection& collection) {
    struct MutableRank {
        std::uint64_t record_count = 0;
        std::uint64_t payload_bytes = 0;
    };
    std::map<protocol::DeviceId, MutableRank> by_device;
    std::uint64_t total_payload = 0;
    for (const auto& ref : collection.records) {
        auto& rank = by_device[ref.device];
        ++rank.record_count;
        rank.payload_bytes += ref.payload_size;
        total_payload += ref.payload_size;
    }

    std::vector<DevicePayloadRank> ranking;
    ranking.reserve(by_device.size());
    for (const auto& [device, mutable_rank] : by_device) {
        DevicePayloadRank rank;
        rank.device = device;
        rank.record_count = mutable_rank.record_count;
        rank.payload_bytes = mutable_rank.payload_bytes;
        if (total_payload != 0) {
            rank.payload_share = static_cast<double>(mutable_rank.payload_bytes)
                / static_cast<double>(total_payload);
        }
        if (mutable_rank.record_count != 0) {
            rank.average_payload_size = static_cast<double>(mutable_rank.payload_bytes)
                / static_cast<double>(mutable_rank.record_count);
        }
        ranking.push_back(rank);
    }
    std::sort(
        ranking.begin(),
        ranking.end(),
        [](const DevicePayloadRank& left, const DevicePayloadRank& right) {
            if (left.payload_bytes != right.payload_bytes) {
                return left.payload_bytes > right.payload_bytes;
            }
            if (left.record_count != right.record_count) {
                return left.record_count > right.record_count;
            }
            return left.device < right.device;
        });
    return ranking;
}

ArchiveDensitySummary calculate_archive_density(const ArchiveCollection& collection) {
    ArchiveDensitySummary density;
    density.archive_count = collection.archives.size();
    density.record_count = collection.records.size();
    for (const auto& ref : collection.records) {
        density.payload_bytes += ref.payload_size;
    }
    if (density.archive_count != 0) {
        density.records_per_archive = static_cast<double>(density.record_count)
            / static_cast<double>(density.archive_count);
        density.payload_bytes_per_archive = static_cast<double>(density.payload_bytes)
            / static_cast<double>(density.archive_count);
    }
    if (density.record_count != 0) {
        density.payload_bytes_per_record = static_cast<double>(density.payload_bytes)
            / static_cast<double>(density.record_count);
    }
    return density;
}

bool archive_density_empty(const ArchiveDensitySummary& density) {
    return density.archive_count == 0
        && density.record_count == 0
        && density.payload_bytes == 0;
}

bool archive_statistics_report_empty(const ArchiveStatisticsReport& report) {
    return report.summary.record_count == 0
        && report.time_buckets.empty()
        && report.device_timelines.empty()
        && report.gaps.empty()
        && report.overlaps.empty()
        && report.sequence_anomalies.empty()
        && report.payload_ranking.empty()
        && report.device_kind_matrix.empty()
        && archive_density_empty(report.density);
}

bool archive_statistics_report_has_findings(const ArchiveStatisticsReport& report) {
    return !report.gaps.empty()
        || !report.overlaps.empty()
        || !report.sequence_anomalies.empty();
}

std::vector<DeviceKindMatrixCell> build_device_kind_matrix(const ArchiveCollection& collection) {
    std::map<std::pair<protocol::DeviceId, protocol::PacketKind>, std::uint64_t> counts;
    for (const auto& ref : collection.records) {
        auto record = load_collection_record(ref);
        if (!record) {
            continue;
        }
        ++counts[{ref.device, record->packet.kind}];
    }
    std::vector<DeviceKindMatrixCell> cells;
    cells.reserve(counts.size());
    for (const auto& [key, count] : counts) {
        cells.push_back(DeviceKindMatrixCell{key.first, key.second, count});
    }
    std::sort(
        cells.begin(),
        cells.end(),
        [](const DeviceKindMatrixCell& left, const DeviceKindMatrixCell& right) {
            if (left.device != right.device) {
                return left.device < right.device;
            }
            return static_cast<unsigned>(left.kind) < static_cast<unsigned>(right.kind);
        });
    return cells;
}

ArchiveStatisticsReport build_archive_statistics_report(const ArchiveCollection& collection,
                                                        std::uint64_t bucket_width_ns,
                                                        std::uint64_t gap_threshold_ns) {
    ArchiveStatisticsReport report;
    report.summary = summarize_collection(collection);
    report.time_buckets = build_archive_time_buckets(collection, bucket_width_ns);
    report.device_timelines = build_device_timelines(collection);
    report.gaps = detect_archive_gaps(collection, gap_threshold_ns);
    report.overlaps = detect_archive_overlaps(collection);
    report.sequence_anomalies = detect_sequence_anomalies(collection);
    report.payload_ranking = rank_devices_by_payload(collection);
    report.device_kind_matrix = build_device_kind_matrix(collection);
    report.payload_percentiles = calculate_payload_percentiles(collection);
    report.density = calculate_archive_density(collection);
    return report;
}

std::string render_archive_time_buckets(const std::vector<ArchiveTimeBucket>& buckets) {
    std::ostringstream out;
    out << "archive_time_buckets\n";
    for (const auto& bucket : buckets) {
        out << "  bucket: start=" << bucket.bucket_start_ns
            << " end=" << bucket.bucket_end_ns
            << " records=" << bucket.record_count
            << " payload_bytes=" << bucket.payload_bytes
            << " archives=" << bucket.archive_count
            << "\n";
    }
    return out.str();
}

std::string render_device_timelines(const std::vector<DeviceTimeline>& timelines) {
    std::ostringstream out;
    out << "device_timelines\n";
    for (const auto& timeline : timelines) {
        out << "  device: " << timeline.device
            << " points=" << timeline.points.size()
            << " first_time_ns=" << timeline.first_time_ns
            << " last_time_ns=" << timeline.last_time_ns
            << " payload_bytes=" << timeline.total_payload_bytes
            << "\n";
        for (const auto& point : timeline.points) {
            out << "    point: time=" << point.capture_time_ns
                << " sequence=" << point.sequence
                << " payload_size=" << point.payload_size
                << " archive=" << point.archive_path.string()
                << "\n";
        }
    }
    return out.str();
}

std::string render_archive_gaps(const std::vector<ArchiveGap>& gaps) {
    std::ostringstream out;
    out << "archive_gaps\n";
    for (const auto& gap : gaps) {
        out << "  gap: device=" << gap.device
            << " previous=" << gap.previous_time_ns
            << " next=" << gap.next_time_ns
            << " gap_ns=" << gap.gap_ns
            << " previous_archive=" << gap.previous_archive.string()
            << " next_archive=" << gap.next_archive.string()
            << "\n";
    }
    return out.str();
}

std::string render_archive_overlaps(const std::vector<ArchiveOverlap>& overlaps) {
    std::ostringstream out;
    out << "archive_overlaps\n";
    for (const auto& overlap : overlaps) {
        out << "  overlap: left=" << overlap.left_archive.string()
            << " right=" << overlap.right_archive.string()
            << " start=" << overlap.overlap_start_ns
            << " end=" << overlap.overlap_end_ns
            << " span_ns=" << overlap.overlap_span_ns
            << " left_records=" << overlap.left_records_in_overlap
            << " right_records=" << overlap.right_records_in_overlap
            << "\n";
    }
    return out.str();
}

std::string render_sequence_anomalies(const std::vector<SequenceAnomaly>& anomalies) {
    std::ostringstream out;
    out << "sequence_anomalies\n";
    for (const auto& anomaly : anomalies) {
        out << "  anomaly: device=" << anomaly.device
            << " previous_sequence=" << anomaly.previous_sequence
            << " current_sequence=" << anomaly.current_sequence
            << " capture_time_ns=" << anomaly.capture_time_ns
            << " archive=" << anomaly.archive_path.string()
            << " reason=\"" << anomaly.reason << "\"\n";
    }
    return out.str();
}

std::string render_payload_percentiles(const PayloadPercentiles& percentiles) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "payload_percentiles\n"
        << "  minimum: " << percentiles.minimum << "\n"
        << "  p50: " << percentiles.p50 << "\n"
        << "  p90: " << percentiles.p90 << "\n"
        << "  p99: " << percentiles.p99 << "\n"
        << "  maximum: " << percentiles.maximum << "\n"
        << "  average: " << percentiles.average << "\n";
    return out.str();
}

std::string render_device_payload_ranking(const std::vector<DevicePayloadRank>& ranking) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << "device_payload_ranking\n";
    for (const auto& rank : ranking) {
        out << "  device: " << rank.device
            << " records=" << rank.record_count
            << " payload_bytes=" << rank.payload_bytes
            << " payload_share=" << rank.payload_share
            << " average_payload_size=" << rank.average_payload_size
            << "\n";
    }
    return out.str();
}

std::string render_archive_density(const ArchiveDensitySummary& density) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "archive_density\n"
        << "  archive_count: " << density.archive_count << "\n"
        << "  record_count: " << density.record_count << "\n"
        << "  payload_bytes: " << density.payload_bytes << "\n"
        << "  records_per_archive: " << density.records_per_archive << "\n"
        << "  payload_bytes_per_archive: " << density.payload_bytes_per_archive << "\n"
        << "  payload_bytes_per_record: " << density.payload_bytes_per_record << "\n";
    return out.str();
}

std::string render_device_kind_matrix(const std::vector<DeviceKindMatrixCell>& matrix) {
    std::ostringstream out;
    out << "device_kind_matrix\n";
    for (const auto& cell : matrix) {
        out << "  cell: device=" << cell.device
            << " kind=" << protocol::packet_kind_name(cell.kind)
            << " records=" << cell.record_count
            << "\n";
    }
    return out.str();
}

std::string render_archive_statistics_report(const ArchiveStatisticsReport& report) {
    std::ostringstream out;
    out << "archive_statistics\n"
        << "  records: " << report.summary.record_count << "\n"
        << "  first_time_ns: " << report.summary.first_time_ns << "\n"
        << "  last_time_ns: " << report.summary.last_time_ns << "\n"
        << "  time_buckets: " << report.time_buckets.size() << "\n"
        << "  device_timelines: " << report.device_timelines.size() << "\n"
        << "  gaps: " << report.gaps.size() << "\n"
        << "  overlaps: " << report.overlaps.size() << "\n"
        << "  sequence_anomalies: " << report.sequence_anomalies.size() << "\n"
        << "  payload_ranking: " << report.payload_ranking.size() << "\n"
        << "  matrix_cells: " << report.device_kind_matrix.size() << "\n"
        << "  density_archives: " << report.density.archive_count << "\n";
    out << render_payload_percentiles(report.payload_percentiles);
    out << render_device_payload_ranking(report.payload_ranking);
    out << render_archive_density(report.density);
    return out.str();
}

} // namespace aethon::storage
