#include "aethon/storage/archive_collection.hpp"

#include "aethon/common/error.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

ArchiveCollectionRecordRef make_ref(const ArchiveCollectionEntry& entry,
                                    const ArchiveIndexEntry& index_entry) {
    return ArchiveCollectionRecordRef{
        entry.path,
        entry.ordinal,
        index_entry.offset,
        index_entry.capture_time_ns,
        index_entry.payload_size,
        index_entry.device,
        index_entry.sequence,
    };
}

bool ref_matches_query(const ArchiveCollectionRecordRef& ref, const ArchiveQuery& query, ArchiveQueryStats* stats) {
    if (stats != nullptr) {
        ++stats->scanned_records;
    }
    if (query.start_time_ns && ref.capture_time_ns < *query.start_time_ns) {
        if (stats != nullptr) {
            ++stats->skipped_by_time;
        }
        return false;
    }
    if (query.end_time_ns && ref.capture_time_ns > *query.end_time_ns) {
        if (stats != nullptr) {
            ++stats->skipped_by_time;
        }
        return false;
    }
    if (query.device && ref.device != *query.device) {
        if (stats != nullptr) {
            ++stats->skipped_by_device;
        }
        return false;
    }
    if (query.min_payload_size && ref.payload_size < *query.min_payload_size) {
        if (stats != nullptr) {
            ++stats->skipped_by_payload;
        }
        return false;
    }
    if (query.max_payload_size && ref.payload_size > *query.max_payload_size) {
        if (stats != nullptr) {
            ++stats->skipped_by_payload;
        }
        return false;
    }
    if (stats != nullptr) {
        ++stats->matched_records;
    }
    return true;
}

void add_record_ref(ArchiveCollection& collection, const ArchiveCollectionRecordRef& ref) {
    collection.records.push_back(ref);
    collection.records_by_device[ref.device].push_back(ref);
    if (collection.summary.record_count == 0) {
        collection.summary.first_time_ns = ref.capture_time_ns;
        collection.summary.last_time_ns = ref.capture_time_ns;
    } else {
        collection.summary.first_time_ns = std::min(collection.summary.first_time_ns, ref.capture_time_ns);
        collection.summary.last_time_ns = std::max(collection.summary.last_time_ns, ref.capture_time_ns);
    }
    ++collection.summary.record_count;
}

void sort_collection(ArchiveCollection& collection) {
    auto by_time = [](const ArchiveCollectionRecordRef& left, const ArchiveCollectionRecordRef& right) {
        if (left.capture_time_ns != right.capture_time_ns) {
            return left.capture_time_ns < right.capture_time_ns;
        }
        if (left.archive_ordinal != right.archive_ordinal) {
            return left.archive_ordinal < right.archive_ordinal;
        }
        return left.offset < right.offset;
    };
    std::stable_sort(collection.records.begin(), collection.records.end(), by_time);
    for (auto& [device, refs] : collection.records_by_device) {
        (void)device;
        std::stable_sort(refs.begin(), refs.end(), by_time);
    }
}

std::optional<ArchiveRecord> load_record_at_offset(const std::filesystem::path& path, std::uint64_t offset) {
    ArchiveReader reader(path);
    while (auto record = reader.next()) {
        if (record->offset == offset) {
            return record;
        }
        if (record->offset > offset) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::uint64_t count_archives_for_refs(const std::vector<ArchiveCollectionRecordRef>& refs) {
    std::set<std::uint64_t> ordinals;
    for (const auto& ref : refs) {
        ordinals.insert(ref.archive_ordinal);
    }
    return ordinals.size();
}

} // namespace

ArchiveCollection build_archive_collection(const std::vector<std::filesystem::path>& paths) {
    std::vector<ArchiveCollectionEntry> entries;
    entries.reserve(paths.size());
    std::uint64_t ordinal = 0;
    for (const auto& path : paths) {
        ArchiveCollectionEntry entry;
        entry.path = path;
        entry.manifest = build_archive_manifest(path);
        entry.index = build_archive_index(path);
        entry.ordinal = ordinal++;
        entries.push_back(std::move(entry));
    }
    return build_archive_collection(std::move(entries));
}

ArchiveCollection build_archive_collection(std::vector<ArchiveCollectionEntry> entries) {
    ArchiveCollection collection;
    collection.summary.format_version = 1;
    collection.archives = std::move(entries);
    for (const auto& archive : collection.archives) {
        for (const auto& record : archive.index.records) {
            add_record_ref(collection, make_ref(archive, record));
        }
    }
    sort_collection(collection);
    return collection;
}

std::vector<ArchiveCollectionRecordRef> filter_collection_refs(const ArchiveCollection& collection,
                                                              const ArchiveQuery& query) {
    std::vector<ArchiveCollectionRecordRef> refs;
    const auto* source = &collection.records;
    if (query.device) {
        auto it = collection.records_by_device.find(*query.device);
        if (it == collection.records_by_device.end()) {
            return refs;
        }
        source = &it->second;
    }
    refs.reserve(source->size());
    for (const auto& ref : *source) {
        if (ref_matches_query(ref, query, nullptr)) {
            refs.push_back(ref);
            if (query.limit != 0 && refs.size() >= query.limit) {
                break;
            }
        }
    }
    return refs;
}

CollectionSearchResult search_archive_collection(const ArchiveCollection& collection,
                                                 const CollectionSearchOptions& options) {
    CollectionSearchResult result;
    const auto* source = &collection.records;
    if (options.query.device) {
        auto it = collection.records_by_device.find(*options.query.device);
        if (it == collection.records_by_device.end()) {
            return result;
        }
        source = &it->second;
    }

    for (const auto& ref : *source) {
        if (!ref_matches_query(ref, options.query, &result.stats)) {
            continue;
        }
        if (options.query.kind) {
            auto record = load_collection_record(ref);
            if (!record || record->packet.kind != *options.query.kind) {
                --result.stats.matched_records;
                ++result.stats.skipped_by_kind;
                continue;
            }
            result.refs.push_back(ref);
            if (options.load_records) {
                result.records.push_back(std::move(*record));
            }
            if (options.query.limit != 0 && result.refs.size() >= options.query.limit) {
                break;
            }
            continue;
        }
        result.refs.push_back(ref);
        if (options.load_records) {
            auto record = load_collection_record(ref);
            if (record) {
                result.records.push_back(std::move(*record));
            }
        }
        if (options.query.limit != 0 && result.refs.size() >= options.query.limit) {
            break;
        }
    }

    if (options.sort_by_time) {
        std::stable_sort(
            result.records.begin(),
            result.records.end(),
            [](const ArchiveRecord& left, const ArchiveRecord& right) {
                return left.capture_time_ns < right.capture_time_ns;
            });
        std::stable_sort(
            result.refs.begin(),
            result.refs.end(),
            [](const ArchiveCollectionRecordRef& left, const ArchiveCollectionRecordRef& right) {
                return left.capture_time_ns < right.capture_time_ns;
            });
    }
    return result;
}

std::optional<ArchiveCollectionRecordRef> collection_record_at_or_after(const ArchiveCollection& collection,
                                                                       std::uint64_t capture_time_ns) {
    auto it = std::lower_bound(
        collection.records.begin(),
        collection.records.end(),
        capture_time_ns,
        [](const ArchiveCollectionRecordRef& ref, std::uint64_t ts) {
            return ref.capture_time_ns < ts;
        });
    if (it == collection.records.end()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<ArchiveRecord> load_collection_record(const ArchiveCollectionRecordRef& ref) {
    auto record = load_record_at_offset(ref.path, ref.offset);
    if (!record) {
        return std::nullopt;
    }
    if (record->capture_time_ns != ref.capture_time_ns
        || record->packet.device != ref.device
        || record->packet.sequence != ref.sequence) {
        throw Error(ErrorCode::archive_corrupt, "collection record reference does not match archive contents");
    }
    return record;
}

std::vector<CollectionCoverageWindow> collection_device_coverage(const ArchiveCollection& collection) {
    std::vector<CollectionCoverageWindow> windows;
    windows.reserve(collection.records_by_device.size());
    for (const auto& [device, refs] : collection.records_by_device) {
        if (refs.empty()) {
            continue;
        }
        CollectionCoverageWindow window;
        window.device = device;
        window.first_time_ns = refs.front().capture_time_ns;
        window.last_time_ns = refs.front().capture_time_ns;
        window.record_count = refs.size();
        for (const auto& ref : refs) {
            window.first_time_ns = std::min(window.first_time_ns, ref.capture_time_ns);
            window.last_time_ns = std::max(window.last_time_ns, ref.capture_time_ns);
        }
        window.archive_count = count_archives_for_refs(refs);
        windows.push_back(window);
    }
    std::sort(
        windows.begin(),
        windows.end(),
        [](const CollectionCoverageWindow& left, const CollectionCoverageWindow& right) {
            if (left.record_count != right.record_count) {
                return left.record_count > right.record_count;
            }
            return left.device < right.device;
        });
    return windows;
}

std::vector<std::filesystem::path> collection_archives_for_device(const ArchiveCollection& collection,
                                                                 protocol::DeviceId device) {
    std::vector<std::filesystem::path> paths;
    auto it = collection.records_by_device.find(device);
    if (it == collection.records_by_device.end()) {
        return paths;
    }
    std::set<std::uint64_t> seen_ordinals;
    for (const auto& ref : it->second) {
        if (!seen_ordinals.insert(ref.archive_ordinal).second) {
            continue;
        }
        auto archive_it = std::find_if(
            collection.archives.begin(),
            collection.archives.end(),
            [&](const ArchiveCollectionEntry& entry) {
                return entry.ordinal == ref.archive_ordinal;
            });
        if (archive_it != collection.archives.end()) {
            paths.push_back(archive_it->path);
        }
    }
    return paths;
}

std::string render_archive_collection(const ArchiveCollection& collection) {
    std::ostringstream out;
    out << "archive_collection\n"
        << "  archives: " << collection.archives.size() << "\n"
        << "  records: " << collection.records.size() << "\n"
        << "  first_time_ns: " << collection.summary.first_time_ns << "\n"
        << "  last_time_ns: " << collection.summary.last_time_ns << "\n";
    for (const auto& archive : collection.archives) {
        out << "  archive: " << archive.path.string()
            << " ordinal=" << archive.ordinal
            << " records=" << archive.index.records.size()
            << " first_time_ns=" << archive.manifest.summary.first_time_ns
            << " last_time_ns=" << archive.manifest.summary.last_time_ns
            << "\n";
    }
    return out.str();
}

std::string render_collection_search_result(const CollectionSearchResult& result) {
    std::ostringstream out;
    out << "collection_search_result\n"
        << "  refs: " << result.refs.size() << "\n"
        << "  records: " << result.records.size() << "\n"
        << "  scanned_records: " << result.stats.scanned_records << "\n"
        << "  matched_records: " << result.stats.matched_records << "\n";
    for (const auto& ref : result.refs) {
        out << "  ref: " << ref.path.string()
            << " ordinal=" << ref.archive_ordinal
            << " offset=" << ref.offset
            << " capture_time_ns=" << ref.capture_time_ns
            << " device=" << ref.device
            << " sequence=" << ref.sequence
            << "\n";
    }
    return out.str();
}

std::string render_collection_coverage(const std::vector<CollectionCoverageWindow>& windows) {
    std::ostringstream out;
    out << "collection_coverage\n";
    for (const auto& window : windows) {
        out << "  device: " << window.device
            << " records=" << window.record_count
            << " archives=" << window.archive_count
            << " first_time_ns=" << window.first_time_ns
            << " last_time_ns=" << window.last_time_ns
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
