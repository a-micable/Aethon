#include "aethon/storage/manifest_diff.hpp"

#include "aethon/protocol/inspector.hpp"
#include "aethon/storage/retention.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace aethon::storage {
namespace {

std::string summary_string(const ArchiveSummary& summary) {
    std::ostringstream out;
    out << "records=" << summary.record_count
        << " first=" << summary.first_time_ns
        << " last=" << summary.last_time_ns
        << " version=" << summary.format_version;
    return out.str();
}

std::string device_string(const DeviceArchiveSummary& device) {
    std::ostringstream out;
    out << "device=" << device.device
        << " records=" << device.record_count
        << " first=" << device.first_time_ns
        << " last=" << device.last_time_ns
        << " payload_bytes=" << device.total_payload_bytes;
    return out.str();
}

std::string kind_string(const PacketKindCount& kind) {
    std::ostringstream out;
    out << "kind=" << protocol::packet_kind_name(kind.kind)
        << " records=" << kind.count;
    return out.str();
}

ManifestDiffEntry make_entry(ManifestDiffKind kind,
                             std::string key,
                             std::string before,
                             std::string after,
                             std::int64_t record_delta,
                             std::int64_t payload_delta) {
    ManifestDiffEntry entry;
    entry.kind = kind;
    entry.key = std::move(key);
    entry.before = std::move(before);
    entry.after = std::move(after);
    entry.record_delta = record_delta;
    entry.payload_delta = payload_delta;
    return entry;
}

std::map<protocol::DeviceId, DeviceArchiveSummary> map_devices(const ArchiveManifest& manifest) {
    std::map<protocol::DeviceId, DeviceArchiveSummary> out;
    for (const auto& device : manifest.devices) {
        out[device.device] = device;
    }
    return out;
}

std::map<protocol::PacketKind, PacketKindCount> map_kinds(const ArchiveManifest& manifest) {
    std::map<protocol::PacketKind, PacketKindCount> out;
    for (const auto& kind : manifest.packet_kinds) {
        out[kind.kind] = kind;
    }
    return out;
}

void diff_summary(ManifestDiff& diff) {
    if (diff.before.summary.format_version == diff.after.summary.format_version
        && diff.before.summary.record_count == diff.after.summary.record_count
        && diff.before.summary.first_time_ns == diff.after.summary.first_time_ns
        && diff.before.summary.last_time_ns == diff.after.summary.last_time_ns) {
        return;
    }
    diff.entries.push_back(make_entry(
        ManifestDiffKind::summary_changed,
        "summary",
        summary_string(diff.before.summary),
        summary_string(diff.after.summary),
        static_cast<std::int64_t>(diff.after.summary.record_count)
            - static_cast<std::int64_t>(diff.before.summary.record_count),
        static_cast<std::int64_t>(manifest_payload_bytes(diff.after))
            - static_cast<std::int64_t>(manifest_payload_bytes(diff.before))));
}

void diff_devices(ManifestDiff& diff) {
    auto before = map_devices(diff.before);
    auto after = map_devices(diff.after);
    std::set<protocol::DeviceId> keys;
    for (const auto& [device, summary] : before) {
        (void)summary;
        keys.insert(device);
    }
    for (const auto& [device, summary] : after) {
        (void)summary;
        keys.insert(device);
    }
    for (auto device : keys) {
        auto b = before.find(device);
        auto a = after.find(device);
        auto key = std::to_string(device);
        if (b == before.end()) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::device_added,
                key,
                "",
                device_string(a->second),
                static_cast<std::int64_t>(a->second.record_count),
                static_cast<std::int64_t>(a->second.total_payload_bytes)));
            continue;
        }
        if (a == after.end()) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::device_removed,
                key,
                device_string(b->second),
                "",
                -static_cast<std::int64_t>(b->second.record_count),
                -static_cast<std::int64_t>(b->second.total_payload_bytes)));
            continue;
        }
        if (device_string(b->second) != device_string(a->second)) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::device_changed,
                key,
                device_string(b->second),
                device_string(a->second),
                static_cast<std::int64_t>(a->second.record_count)
                    - static_cast<std::int64_t>(b->second.record_count),
                static_cast<std::int64_t>(a->second.total_payload_bytes)
                    - static_cast<std::int64_t>(b->second.total_payload_bytes)));
        }
    }
}

void diff_kinds(ManifestDiff& diff) {
    auto before = map_kinds(diff.before);
    auto after = map_kinds(diff.after);
    std::set<protocol::PacketKind> keys;
    for (const auto& [kind, count] : before) {
        (void)count;
        keys.insert(kind);
    }
    for (const auto& [kind, count] : after) {
        (void)count;
        keys.insert(kind);
    }
    for (auto kind : keys) {
        auto b = before.find(kind);
        auto a = after.find(kind);
        auto key = protocol::packet_kind_name(kind);
        if (b == before.end()) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::kind_added,
                key,
                "",
                kind_string(a->second),
                static_cast<std::int64_t>(a->second.count),
                0));
            continue;
        }
        if (a == after.end()) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::kind_removed,
                key,
                kind_string(b->second),
                "",
                -static_cast<std::int64_t>(b->second.count),
                0));
            continue;
        }
        if (b->second.count != a->second.count) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::kind_changed,
                key,
                kind_string(b->second),
                kind_string(a->second),
                static_cast<std::int64_t>(a->second.count)
                    - static_cast<std::int64_t>(b->second.count),
                0));
        }
    }
}

void diff_warnings(ManifestDiff& diff) {
    std::set<std::string> before(diff.before.warnings.begin(), diff.before.warnings.end());
    std::set<std::string> after(diff.after.warnings.begin(), diff.after.warnings.end());
    for (const auto& warning : before) {
        if (!after.contains(warning)) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::warning_removed,
                warning,
                warning,
                "",
                0,
                0));
        }
    }
    for (const auto& warning : after) {
        if (!before.contains(warning)) {
            diff.entries.push_back(make_entry(
                ManifestDiffKind::warning_added,
                warning,
                "",
                warning,
                0,
                0));
        }
    }
}

} // namespace

std::string manifest_diff_kind_name(ManifestDiffKind kind) {
    switch (kind) {
    case ManifestDiffKind::summary_changed:
        return "summary_changed";
    case ManifestDiffKind::device_added:
        return "device_added";
    case ManifestDiffKind::device_removed:
        return "device_removed";
    case ManifestDiffKind::device_changed:
        return "device_changed";
    case ManifestDiffKind::kind_added:
        return "kind_added";
    case ManifestDiffKind::kind_removed:
        return "kind_removed";
    case ManifestDiffKind::kind_changed:
        return "kind_changed";
    case ManifestDiffKind::warning_added:
        return "warning_added";
    case ManifestDiffKind::warning_removed:
        return "warning_removed";
    }
    return "unknown";
}

ManifestDiff diff_archive_manifests(const ArchiveManifest& before,
                                    const ArchiveManifest& after) {
    ManifestDiff diff;
    diff.before = before;
    diff.after = after;
    diff.record_delta = static_cast<std::int64_t>(after.summary.record_count)
        - static_cast<std::int64_t>(before.summary.record_count);
    diff.payload_delta = static_cast<std::int64_t>(manifest_payload_bytes(after))
        - static_cast<std::int64_t>(manifest_payload_bytes(before));
    diff.device_delta = static_cast<std::int64_t>(after.devices.size())
        - static_cast<std::int64_t>(before.devices.size());
    diff.kind_delta = static_cast<std::int64_t>(after.packet_kinds.size())
        - static_cast<std::int64_t>(before.packet_kinds.size());
    diff_summary(diff);
    diff_devices(diff);
    diff_kinds(diff);
    diff_warnings(diff);
    return diff;
}

bool manifest_diff_empty(const ManifestDiff& diff) {
    return diff.entries.empty();
}

std::vector<ManifestDiffEntry> manifest_diff_entries_by_kind(const ManifestDiff& diff,
                                                            ManifestDiffKind kind) {
    std::vector<ManifestDiffEntry> entries;
    for (const auto& entry : diff.entries) {
        if (entry.kind == kind) {
            entries.push_back(entry);
        }
    }
    return entries;
}

std::string render_manifest_diff_entry(const ManifestDiffEntry& entry) {
    std::ostringstream out;
    out << manifest_diff_kind_name(entry.kind)
        << " key=" << entry.key
        << " record_delta=" << entry.record_delta
        << " payload_delta=" << entry.payload_delta
        << " before=\"" << entry.before
        << "\" after=\"" << entry.after
        << "\"";
    return out.str();
}

std::string render_manifest_diff(const ManifestDiff& diff) {
    std::ostringstream out;
    out << "manifest_diff\n"
        << "  record_delta: " << diff.record_delta << "\n"
        << "  payload_delta: " << diff.payload_delta << "\n"
        << "  device_delta: " << diff.device_delta << "\n"
        << "  kind_delta: " << diff.kind_delta << "\n";
    for (const auto& entry : diff.entries) {
        out << "  entry: " << render_manifest_diff_entry(entry) << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
