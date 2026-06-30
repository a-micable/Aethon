#pragma once

#include "aethon/storage/manifest.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::storage {

enum class ManifestDiffKind {
    summary_changed,
    device_added,
    device_removed,
    device_changed,
    kind_added,
    kind_removed,
    kind_changed,
    warning_added,
    warning_removed,
};

struct ManifestDiffEntry {
    ManifestDiffKind kind = ManifestDiffKind::summary_changed;
    std::string key;
    std::string before;
    std::string after;
    std::int64_t record_delta = 0;
    std::int64_t payload_delta = 0;
};

struct ManifestDiff {
    ArchiveManifest before;
    ArchiveManifest after;
    std::vector<ManifestDiffEntry> entries;
    std::int64_t record_delta = 0;
    std::int64_t payload_delta = 0;
    std::int64_t device_delta = 0;
    std::int64_t kind_delta = 0;
};

[[nodiscard]] std::string manifest_diff_kind_name(ManifestDiffKind kind);
[[nodiscard]] ManifestDiff diff_archive_manifests(const ArchiveManifest& before,
                                                  const ArchiveManifest& after);
[[nodiscard]] bool manifest_diff_empty(const ManifestDiff& diff);
[[nodiscard]] std::vector<ManifestDiffEntry> manifest_diff_entries_by_kind(const ManifestDiff& diff,
                                                                           ManifestDiffKind kind);
[[nodiscard]] std::string render_manifest_diff_entry(const ManifestDiffEntry& entry);
[[nodiscard]] std::string render_manifest_diff(const ManifestDiff& diff);

} // namespace aethon::storage
