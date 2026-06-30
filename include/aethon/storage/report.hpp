#pragma once

#include "aethon/diagnostics/archive_health.hpp"
#include "aethon/storage/manifest.hpp"
#include "aethon/storage/query.hpp"
#include "aethon/storage/retention.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

struct ArchiveReportSection {
    std::string title;
    std::string body;
};

struct ArchiveReport {
    std::filesystem::path path;
    ArchiveManifest manifest;
    diagnostics::ArchiveHealthReport health;
    std::vector<ArchiveReportSection> sections;
};

struct ArchiveReportOptions {
    bool include_manifest = true;
    bool include_health = true;
    bool include_devices = true;
    bool include_packet_kinds = true;
};

[[nodiscard]] ArchiveReport build_archive_report(const std::filesystem::path& path,
                                                 const ArchiveReportOptions& options = {});

[[nodiscard]] std::string render_archive_report(const ArchiveReport& report);
[[nodiscard]] std::string render_report_section(const ArchiveReportSection& section);

} // namespace aethon::storage
