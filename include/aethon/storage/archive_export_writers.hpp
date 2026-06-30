#pragma once

#include "aethon/storage/archive_collection.hpp"
#include "aethon/storage/manifest_diff.hpp"
#include "aethon/storage/archive_sampling.hpp"

#include <filesystem>
#include <iosfwd>
#include <string>

namespace aethon::storage {

enum class ExportFormat {
    csv,
    json_lines,
    text_summary,
};

struct ArchiveExportWriterOptions {
    ExportFormat format = ExportFormat::csv;
    bool include_header = true;
    bool include_archive_path = true;
    bool include_payload_size = true;
    bool include_packet_kind = true;
    bool include_offsets = true;
};

class ArchiveRecordExportWriter {
public:
    explicit ArchiveRecordExportWriter(ArchiveExportWriterOptions options = {});

    void write_header(std::ostream& out) const;
    void write_record(std::ostream& out, const ArchiveRecord& record, const std::filesystem::path& path = {}) const;
    void write_ref(std::ostream& out, const ArchiveCollectionRecordRef& ref) const;
    void write_collection(std::ostream& out, const ArchiveCollection& collection) const;
    void write_search_result(std::ostream& out, const CollectionSearchResult& result) const;

    [[nodiscard]] const ArchiveExportWriterOptions& options() const noexcept { return options_; }

private:
    ArchiveExportWriterOptions options_;
};

class ArchiveSummaryExportWriter {
public:
    explicit ArchiveSummaryExportWriter(ArchiveExportWriterOptions options = {});

    void write_manifest(std::ostream& out, const ArchiveManifest& manifest, const std::filesystem::path& path = {}) const;
    void write_collection(std::ostream& out, const ArchiveCollection& collection) const;
    void write_sample(std::ostream& out, const RecordSample& sample) const;
    void write_manifest_diff(std::ostream& out, const ManifestDiff& diff) const;

private:
    ArchiveExportWriterOptions options_;
};

[[nodiscard]] std::string export_format_name(ExportFormat format);
[[nodiscard]] std::string escape_json_string(const std::string& value);
[[nodiscard]] std::string render_record_json_line(const ArchiveRecord& record,
                                                  const ArchiveExportWriterOptions& options,
                                                  const std::filesystem::path& path = {});
[[nodiscard]] std::string render_ref_json_line(const ArchiveCollectionRecordRef& ref,
                                               const ArchiveExportWriterOptions& options);
[[nodiscard]] std::string render_manifest_json_line(const ArchiveManifest& manifest,
                                                    const ArchiveExportWriterOptions& options,
                                                    const std::filesystem::path& path = {});

} // namespace aethon::storage
