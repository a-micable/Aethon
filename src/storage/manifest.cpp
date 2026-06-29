#include "aethon/storage/manifest.hpp"

#include "aethon/protocol/inspector.hpp"

#include <algorithm>
#include <sstream>

namespace aethon::storage {
namespace {

PacketKindCount* find_kind_count(std::vector<PacketKindCount>& counts, protocol::PacketKind kind) {
    auto it = std::find_if(
        counts.begin(),
        counts.end(),
        [kind](const PacketKindCount& count) {
            return count.kind == kind;
        });
    if (it == counts.end()) {
        counts.push_back(PacketKindCount{kind, 0});
        return &counts.back();
    }
    return &*it;
}

DeviceArchiveSummary* find_device(std::vector<DeviceArchiveSummary>& summaries,
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

void update_device(DeviceArchiveSummary& summary, const ArchiveRecord& record) {
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

std::uint64_t sum_device_records(const std::vector<DeviceArchiveSummary>& devices) {
    std::uint64_t total = 0;
    for (const auto& device : devices) {
        total += device.record_count;
    }
    return total;
}

std::uint64_t sum_kind_records(const std::vector<PacketKindCount>& counts) {
    std::uint64_t total = 0;
    for (const auto& count : counts) {
        total += count.count;
    }
    return total;
}

void sort_manifest(ArchiveManifest& manifest) {
    std::sort(
        manifest.devices.begin(),
        manifest.devices.end(),
        [](const DeviceArchiveSummary& left, const DeviceArchiveSummary& right) {
            if (left.record_count != right.record_count) {
                return left.record_count > right.record_count;
            }
            return left.device < right.device;
        });
    std::sort(
        manifest.packet_kinds.begin(),
        manifest.packet_kinds.end(),
        [](const PacketKindCount& left, const PacketKindCount& right) {
            if (left.count != right.count) {
                return left.count > right.count;
            }
            return static_cast<unsigned>(left.kind) < static_cast<unsigned>(right.kind);
        });
}

} // namespace

ArchiveManifest build_archive_manifest(const std::filesystem::path& path) {
    ArchiveReader reader(path);
    ArchiveManifest manifest;
    auto header_summary = reader.summary();
    manifest.summary.format_version = header_summary.format_version;

    std::uint64_t observed_count = 0;
    std::uint64_t first_observed = 0;
    std::uint64_t last_observed = 0;
    std::uint64_t previous_time = 0;

    while (auto record = reader.next()) {
        if (observed_count == 0) {
            first_observed = record->capture_time_ns;
            last_observed = record->capture_time_ns;
        } else {
            if (record->capture_time_ns < previous_time) {
                manifest.warnings.push_back("archive records are not ordered by capture time");
            }
            first_observed = std::min(first_observed, record->capture_time_ns);
            last_observed = std::max(last_observed, record->capture_time_ns);
        }
        previous_time = record->capture_time_ns;
        ++observed_count;

        auto* device = find_device(manifest.devices, record->packet.device);
        update_device(*device, *record);
        auto* kind = find_kind_count(manifest.packet_kinds, record->packet.kind);
        ++kind->count;
    }

    manifest.summary.record_count = observed_count;
    manifest.summary.first_time_ns = first_observed;
    manifest.summary.last_time_ns = last_observed;

    if (observed_count != header_summary.record_count) {
        manifest.warnings.push_back("archive header record count differs from readable records");
    }
    if (observed_count != 0 && header_summary.first_time_ns != first_observed) {
        manifest.warnings.push_back("archive header first timestamp differs from readable records");
    }
    if (observed_count != 0 && header_summary.last_time_ns != last_observed) {
        manifest.warnings.push_back("archive header last timestamp differs from readable records");
    }

    sort_manifest(manifest);
    auto validation = validate_archive_manifest(manifest);
    manifest.warnings.insert(manifest.warnings.end(), validation.begin(), validation.end());
    return manifest;
}

std::vector<std::string> validate_archive_manifest(const ArchiveManifest& manifest) {
    std::vector<std::string> warnings;
    auto device_total = sum_device_records(manifest.devices);
    auto kind_total = sum_kind_records(manifest.packet_kinds);
    if (device_total != kind_total) {
        warnings.push_back("device and packet-kind aggregates disagree");
    }
    if (manifest.summary.record_count != 0 && device_total != manifest.summary.record_count) {
        warnings.push_back("aggregate record count differs from archive summary");
    }
    if (manifest.summary.record_count == 0 && (!manifest.devices.empty() || !manifest.packet_kinds.empty())) {
        warnings.push_back("empty archive summary has non-empty aggregates");
    }
    for (const auto& device : manifest.devices) {
        if (device.record_count == 0) {
            warnings.push_back("device aggregate has zero records");
        }
        if (device.first_time_ns > device.last_time_ns) {
            warnings.push_back("device aggregate has inverted time range");
        }
    }
    return warnings;
}

std::string render_archive_manifest(const ArchiveManifest& manifest) {
    std::ostringstream out;
    out << "archive_manifest\n"
        << "  records: " << manifest.summary.record_count << "\n"
        << "  first_time_ns: " << manifest.summary.first_time_ns << "\n"
        << "  last_time_ns: " << manifest.summary.last_time_ns << "\n";
    for (const auto& device : manifest.devices) {
        out << "  device: " << device.device
            << " records=" << device.record_count
            << " first=" << device.first_time_ns
            << " last=" << device.last_time_ns
            << " payload_bytes=" << device.total_payload_bytes << "\n";
    }
    for (const auto& kind : manifest.packet_kinds) {
        out << "  kind: " << protocol::packet_kind_name(kind.kind)
            << " records=" << kind.count << "\n";
    }
    for (const auto& warning : manifest.warnings) {
        out << "  warning: " << warning << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
