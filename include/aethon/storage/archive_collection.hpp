#pragma once

#include "aethon/storage/archive.hpp"
#include "aethon/storage/manifest.hpp"
#include "aethon/storage/query.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

struct ArchiveCollectionEntry {
    std::filesystem::path path;
    ArchiveManifest manifest;
    ArchiveIndex index;
    std::uint64_t ordinal = 0;
};

struct ArchiveCollectionRecordRef {
    std::filesystem::path path;
    std::uint64_t archive_ordinal = 0;
    std::uint64_t offset = 0;
    std::uint64_t capture_time_ns = 0;
    std::uint64_t payload_size = 0;
    protocol::DeviceId device = 0;
    std::uint32_t sequence = 0;
};

struct ArchiveCollection {
    std::vector<ArchiveCollectionEntry> archives;
    std::vector<ArchiveCollectionRecordRef> records;
    std::map<protocol::DeviceId, std::vector<ArchiveCollectionRecordRef>> records_by_device;
    ArchiveSummary summary;
};

struct CollectionSearchOptions {
    ArchiveQuery query;
    bool sort_by_time = true;
    bool load_records = true;
};

struct CollectionSearchResult {
    std::vector<ArchiveRecord> records;
    std::vector<ArchiveCollectionRecordRef> refs;
    ArchiveQueryStats stats;
};

struct CollectionCoverageWindow {
    protocol::DeviceId device = 0;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
    std::uint64_t record_count = 0;
    std::uint64_t archive_count = 0;
};

[[nodiscard]] ArchiveCollection build_archive_collection(const std::vector<std::filesystem::path>& paths);
[[nodiscard]] ArchiveCollection build_archive_collection(std::vector<ArchiveCollectionEntry> entries);
[[nodiscard]] std::vector<ArchiveCollectionRecordRef> filter_collection_refs(const ArchiveCollection& collection,
                                                                             const ArchiveQuery& query);
[[nodiscard]] CollectionSearchResult search_archive_collection(const ArchiveCollection& collection,
                                                               const CollectionSearchOptions& options);
[[nodiscard]] std::optional<ArchiveCollectionRecordRef> collection_record_at_or_after(const ArchiveCollection& collection,
                                                                                      std::uint64_t capture_time_ns);
[[nodiscard]] std::optional<ArchiveRecord> load_collection_record(const ArchiveCollectionRecordRef& ref);
[[nodiscard]] std::vector<CollectionCoverageWindow> collection_device_coverage(const ArchiveCollection& collection);
[[nodiscard]] std::vector<std::filesystem::path> collection_archives_for_device(const ArchiveCollection& collection,
                                                                                protocol::DeviceId device);
[[nodiscard]] std::string render_archive_collection(const ArchiveCollection& collection);
[[nodiscard]] std::string render_collection_search_result(const CollectionSearchResult& result);
[[nodiscard]] std::string render_collection_coverage(const std::vector<CollectionCoverageWindow>& windows);

} // namespace aethon::storage
