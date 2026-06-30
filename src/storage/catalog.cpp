#include "aethon/storage/catalog.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

bool overlaps_query_time(const ArchiveManifest& manifest, const ArchiveQuery& query) {
    if (query.start_time_ns && manifest.summary.last_time_ns < *query.start_time_ns) {
        return false;
    }
    if (query.end_time_ns && manifest.summary.first_time_ns > *query.end_time_ns) {
        return false;
    }
    return true;
}

bool contains_device(const ArchiveManifest& manifest, protocol::DeviceId device) {
    return std::any_of(
        manifest.devices.begin(),
        manifest.devices.end(),
        [device](const DeviceArchiveSummary& summary) {
            return summary.device == device;
        });
}

bool contains_kind(const ArchiveManifest& manifest, protocol::PacketKind kind) {
    return std::any_of(
        manifest.packet_kinds.begin(),
        manifest.packet_kinds.end(),
        [kind](const PacketKindCount& count) {
            return count.kind == kind;
        });
}

void sort_entries(std::vector<CatalogEntry>& entries) {
    std::sort(
        entries.begin(),
        entries.end(),
        [](const CatalogEntry& left, const CatalogEntry& right) {
            if (left.manifest.summary.first_time_ns != right.manifest.summary.first_time_ns) {
                return left.manifest.summary.first_time_ns < right.manifest.summary.first_time_ns;
            }
            return left.path.string() < right.path.string();
        });
}

} // namespace

void ArchiveCatalog::add(CatalogEntry entry) {
    remove(entry.path);
    entries_.push_back(std::move(entry));
    sort_entries(entries_);
}

void ArchiveCatalog::remove(const std::filesystem::path& path) {
    entries_.erase(
        std::remove_if(
            entries_.begin(),
            entries_.end(),
            [&](const CatalogEntry& entry) {
                return entry.path == path;
            }),
        entries_.end());
}

void ArchiveCatalog::clear() {
    entries_.clear();
}

const std::vector<CatalogEntry>& ArchiveCatalog::entries() const noexcept {
    return entries_;
}

CatalogSummary ArchiveCatalog::summary() const {
    CatalogSummary summary;
    summary.archive_count = entries_.size();
    std::set<protocol::DeviceId> devices;
    bool have_time = false;
    for (const auto& entry : entries_) {
        summary.record_count += entry.manifest.summary.record_count;
        for (const auto& device : entry.manifest.devices) {
            devices.insert(device.device);
        }
        if (entry.manifest.summary.record_count == 0) {
            continue;
        }
        if (!have_time) {
            summary.first_time_ns = entry.manifest.summary.first_time_ns;
            summary.last_time_ns = entry.manifest.summary.last_time_ns;
            have_time = true;
        } else {
            summary.first_time_ns = std::min(summary.first_time_ns, entry.manifest.summary.first_time_ns);
            summary.last_time_ns = std::max(summary.last_time_ns, entry.manifest.summary.last_time_ns);
        }
    }
    summary.device_count = devices.size();
    return summary;
}

std::vector<CatalogEntry> ArchiveCatalog::select(const ArchiveQuery& query) const {
    std::vector<CatalogEntry> selected;
    for (const auto& entry : entries_) {
        if (!overlaps_query_time(entry.manifest, query)) {
            continue;
        }
        if (query.device && !contains_device(entry.manifest, *query.device)) {
            continue;
        }
        if (query.kind && !contains_kind(entry.manifest, *query.kind)) {
            continue;
        }
        selected.push_back(entry);
        if (query.limit != 0 && selected.size() >= query.limit) {
            break;
        }
    }
    return selected;
}

std::optional<CatalogEntry> ArchiveCatalog::find_covering_time(std::uint64_t time_ns) const {
    for (const auto& entry : entries_) {
        if (entry.manifest.summary.record_count == 0) {
            continue;
        }
        if (entry.manifest.summary.first_time_ns <= time_ns
            && entry.manifest.summary.last_time_ns >= time_ns) {
            return entry;
        }
    }
    return std::nullopt;
}

ArchiveCatalog build_catalog(const std::vector<std::filesystem::path>& paths) {
    ArchiveCatalog catalog;
    for (const auto& path : paths) {
        catalog.add(CatalogEntry{
            path,
            build_archive_manifest(path),
        });
    }
    return catalog;
}

std::string render_catalog_summary(const CatalogSummary& summary) {
    std::ostringstream out;
    out << "archive_catalog\n"
        << "  archives: "
        << summary.archive_count
        << "\n"
        << "  records: "
        << summary.record_count
        << "\n"
        << "  devices: "
        << summary.device_count
        << "\n"
        << "  first_time_ns: "
        << summary.first_time_ns
        << "\n"
        << "  last_time_ns: "
        << summary.last_time_ns
        << "\n";
    return out.str();
}

std::string render_catalog_entries(const ArchiveCatalog& catalog) {
    std::ostringstream out;
    out << render_catalog_summary(catalog.summary());
    for (const auto& entry : catalog.entries()) {
        out << "  entry: "
            << entry.path.string()
            << " records="
            << entry.manifest.summary.record_count
            << " first="
            << entry.manifest.summary.first_time_ns
            << " last="
            << entry.manifest.summary.last_time_ns
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
