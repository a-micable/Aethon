#pragma once

#include "aethon/storage/archive.hpp"
#include "aethon/storage/manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

enum class AuditSeverity {
    info,
    warning,
    error,
};

enum class AuditCheck {
    readable,
    header_matches_records,
    ordered_timestamps,
    monotonic_device_sequences,
    payload_bounds,
    duplicate_offsets,
    manifest_consistency,
    sparse_devices,
};

struct AuditOptions {
    bool require_time_order = true;
    bool require_device_sequence_monotonicity = false;
    std::uint64_t min_payload_size = 0;
    std::uint64_t max_payload_size = 0;
    std::uint64_t sparse_device_record_threshold = 0;
    std::uint64_t sparse_device_payload_threshold = 0;
};

struct AuditFinding {
    AuditSeverity severity = AuditSeverity::info;
    AuditCheck check = AuditCheck::readable;
    std::filesystem::path path;
    std::uint64_t offset = 0;
    std::uint64_t capture_time_ns = 0;
    protocol::DeviceId device = 0;
    std::uint32_t sequence = 0;
    std::string message;
};

struct ArchiveAuditReport {
    std::filesystem::path path;
    ArchiveManifest manifest;
    std::vector<AuditFinding> findings;
    std::uint64_t records_scanned = 0;
    std::uint64_t bytes_estimated = 0;
    std::uint64_t min_payload_size = 0;
    std::uint64_t max_payload_size = 0;
    std::uint64_t first_offset = 0;
    std::uint64_t last_offset = 0;
    bool readable = true;
};

struct ArchiveFleetAuditReport {
    std::vector<ArchiveAuditReport> archives;
    std::vector<AuditFinding> fleet_findings;
    std::uint64_t archives_scanned = 0;
    std::uint64_t records_scanned = 0;
    std::uint64_t error_count = 0;
    std::uint64_t warning_count = 0;
};

[[nodiscard]] std::string audit_severity_name(AuditSeverity severity);
[[nodiscard]] std::string audit_check_name(AuditCheck check);
[[nodiscard]] bool audit_report_ok(const ArchiveAuditReport& report);
[[nodiscard]] bool fleet_audit_ok(const ArchiveFleetAuditReport& report);
[[nodiscard]] ArchiveAuditReport audit_archive_integrity(const std::filesystem::path& path,
                                                         const AuditOptions& options = {});
[[nodiscard]] ArchiveFleetAuditReport audit_archive_fleet(const std::vector<std::filesystem::path>& paths,
                                                          const AuditOptions& options = {});
[[nodiscard]] std::vector<AuditFinding> findings_by_severity(const ArchiveAuditReport& report,
                                                             AuditSeverity severity);
[[nodiscard]] std::vector<AuditFinding> fleet_findings_by_severity(const ArchiveFleetAuditReport& report,
                                                                   AuditSeverity severity);
[[nodiscard]] std::string render_audit_finding(const AuditFinding& finding);
[[nodiscard]] std::string render_archive_audit_report(const ArchiveAuditReport& report);
[[nodiscard]] std::string render_archive_fleet_audit_report(const ArchiveFleetAuditReport& report);

} // namespace aethon::storage
