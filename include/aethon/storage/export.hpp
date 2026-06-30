#pragma once

#include "aethon/storage/archive.hpp"
#include "aethon/storage/manifest.hpp"

#include <filesystem>
#include <ostream>
#include <string>

namespace aethon::storage {

struct ExportOptions {
    bool include_header = true;
    bool include_payload_size = true;
    bool include_kind = true;
};

void export_archive_records(std::ostream& out,
                            const std::filesystem::path& path,
                            const ExportOptions& options = {});

void export_manifest_devices(std::ostream& out,
                             const ArchiveManifest& manifest,
                             const ExportOptions& options = {});

[[nodiscard]] std::string render_record_csv_header(const ExportOptions& options);
[[nodiscard]] std::string render_record_csv_row(const ArchiveRecord& record,
                                                const ExportOptions& options);

} // namespace aethon::storage
