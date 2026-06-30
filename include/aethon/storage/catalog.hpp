#pragma once

#include "aethon/storage/manifest.hpp"
#include "aethon/storage/query.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

struct CatalogEntry {
    std::filesystem::path path;
    ArchiveManifest manifest;
};

struct CatalogSummary {
    std::uint64_t archive_count = 0;
    std::uint64_t record_count = 0;
    std::uint64_t device_count = 0;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
};

class ArchiveCatalog {
public:
    void add(CatalogEntry entry);
    void remove(const std::filesystem::path& path);
    void clear();

    [[nodiscard]] const std::vector<CatalogEntry>& entries() const noexcept;
    [[nodiscard]] CatalogSummary summary() const;
    [[nodiscard]] std::vector<CatalogEntry> select(const ArchiveQuery& query) const;
    [[nodiscard]] std::optional<CatalogEntry> find_covering_time(std::uint64_t time_ns) const;

private:
    std::vector<CatalogEntry> entries_;
};

[[nodiscard]] ArchiveCatalog build_catalog(const std::vector<std::filesystem::path>& paths);
[[nodiscard]] std::string render_catalog_summary(const CatalogSummary& summary);
[[nodiscard]] std::string render_catalog_entries(const ArchiveCatalog& catalog);

} // namespace aethon::storage
