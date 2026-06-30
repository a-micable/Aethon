#include "aethon/storage/archive_audit.hpp"

#include "aethon/common/error.hpp"
#include "aethon/storage/archive_rotation.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace aethon::storage {
namespace {

AuditFinding make_finding(AuditSeverity severity,
                          AuditCheck check,
                          const std::filesystem::path& path,
                          std::string message) {
    AuditFinding finding;
    finding.severity = severity;
    finding.check = check;
    finding.path = path;
    finding.message = std::move(message);
    return finding;
}

AuditFinding make_record_finding(AuditSeverity severity,
                                 AuditCheck check,
                                 const std::filesystem::path& path,
                                 const ArchiveRecord& record,
                                 std::string message) {
    AuditFinding finding = make_finding(severity, check, path, std::move(message));
    finding.offset = record.offset;
    finding.capture_time_ns = record.capture_time_ns;
    finding.device = record.packet.device;
    finding.sequence = record.packet.sequence;
    return finding;
}

void add_header_findings(ArchiveAuditReport& report, const ArchiveSummary& header) {
    const auto& observed = report.manifest.summary;
    if (header.record_count != observed.record_count) {
        report.findings.push_back(make_finding(
            AuditSeverity::error,
            AuditCheck::header_matches_records,
            report.path,
            "archive header record count differs from readable records"));
    }
    if (observed.record_count != 0 && header.first_time_ns != observed.first_time_ns) {
        report.findings.push_back(make_finding(
            AuditSeverity::warning,
            AuditCheck::header_matches_records,
            report.path,
            "archive header first timestamp differs from observed first timestamp"));
    }
    if (observed.record_count != 0 && header.last_time_ns != observed.last_time_ns) {
        report.findings.push_back(make_finding(
            AuditSeverity::warning,
            AuditCheck::header_matches_records,
            report.path,
            "archive header last timestamp differs from observed last timestamp"));
    }
}

void add_manifest_findings(ArchiveAuditReport& report) {
    for (const auto& warning : report.manifest.warnings) {
        report.findings.push_back(make_finding(
            AuditSeverity::warning,
            AuditCheck::manifest_consistency,
            report.path,
            warning));
    }
}

void check_payload_bounds(ArchiveAuditReport& report,
                          const ArchiveRecord& record,
                          const AuditOptions& options) {
    auto payload_size = static_cast<std::uint64_t>(record.packet.payload.size());
    if (report.records_scanned == 0) {
        report.min_payload_size = payload_size;
        report.max_payload_size = payload_size;
    } else {
        report.min_payload_size = std::min(report.min_payload_size, payload_size);
        report.max_payload_size = std::max(report.max_payload_size, payload_size);
    }
    if (options.min_payload_size != 0 && payload_size < options.min_payload_size) {
        report.findings.push_back(make_record_finding(
            AuditSeverity::warning,
            AuditCheck::payload_bounds,
            report.path,
            record,
            "record payload is below configured minimum"));
    }
    if (options.max_payload_size != 0 && payload_size > options.max_payload_size) {
        report.findings.push_back(make_record_finding(
            AuditSeverity::error,
            AuditCheck::payload_bounds,
            report.path,
            record,
            "record payload exceeds configured maximum"));
    }
}

void check_time_order(ArchiveAuditReport& report,
                      const ArchiveRecord& record,
                      bool has_previous,
                      std::uint64_t previous_time) {
    if (has_previous && record.capture_time_ns < previous_time) {
        report.findings.push_back(make_record_finding(
            AuditSeverity::error,
            AuditCheck::ordered_timestamps,
            report.path,
            record,
            "record capture timestamp moved backwards"));
    }
}

void check_device_sequence(ArchiveAuditReport& report,
                           const ArchiveRecord& record,
                           std::map<protocol::DeviceId, std::uint32_t>& last_sequence) {
    auto it = last_sequence.find(record.packet.device);
    if (it != last_sequence.end() && record.packet.sequence < it->second) {
        report.findings.push_back(make_record_finding(
            AuditSeverity::warning,
            AuditCheck::monotonic_device_sequences,
            report.path,
            record,
            "device sequence moved backwards"));
    }
    auto& slot = last_sequence[record.packet.device];
    slot = std::max(slot, record.packet.sequence);
}

void check_duplicate_offset(ArchiveAuditReport& report,
                            const ArchiveRecord& record,
                            std::set<std::uint64_t>& offsets) {
    auto inserted = offsets.insert(record.offset);
    if (!inserted.second) {
        report.findings.push_back(make_record_finding(
            AuditSeverity::error,
            AuditCheck::duplicate_offsets,
            report.path,
            record,
            "archive reader returned the same record offset twice"));
    }
}

void add_sparse_device_findings(ArchiveAuditReport& report, const AuditOptions& options) {
    if (options.sparse_device_record_threshold == 0
        && options.sparse_device_payload_threshold == 0) {
        return;
    }
    for (const auto& device : report.manifest.devices) {
        bool sparse_by_records = options.sparse_device_record_threshold != 0
            && device.record_count < options.sparse_device_record_threshold;
        bool sparse_by_payload = options.sparse_device_payload_threshold != 0
            && device.total_payload_bytes < options.sparse_device_payload_threshold;
        if (!sparse_by_records && !sparse_by_payload) {
            continue;
        }
        AuditFinding finding = make_finding(
            AuditSeverity::info,
            AuditCheck::sparse_devices,
            report.path,
            "device has sparse archive coverage");
        finding.device = device.device;
        finding.capture_time_ns = device.last_time_ns;
        report.findings.push_back(std::move(finding));
    }
}

void accumulate_fleet_counts(ArchiveFleetAuditReport& fleet, const ArchiveAuditReport& archive) {
    ++fleet.archives_scanned;
    fleet.records_scanned += archive.records_scanned;
    for (const auto& finding : archive.findings) {
        if (finding.severity == AuditSeverity::error) {
            ++fleet.error_count;
        }
        if (finding.severity == AuditSeverity::warning) {
            ++fleet.warning_count;
        }
    }
}

} // namespace

std::string audit_severity_name(AuditSeverity severity) {
    switch (severity) {
    case AuditSeverity::info:
        return "info";
    case AuditSeverity::warning:
        return "warning";
    case AuditSeverity::error:
        return "error";
    }
    return "unknown";
}

std::string audit_check_name(AuditCheck check) {
    switch (check) {
    case AuditCheck::readable:
        return "readable";
    case AuditCheck::header_matches_records:
        return "header_matches_records";
    case AuditCheck::ordered_timestamps:
        return "ordered_timestamps";
    case AuditCheck::monotonic_device_sequences:
        return "monotonic_device_sequences";
    case AuditCheck::payload_bounds:
        return "payload_bounds";
    case AuditCheck::duplicate_offsets:
        return "duplicate_offsets";
    case AuditCheck::manifest_consistency:
        return "manifest_consistency";
    case AuditCheck::sparse_devices:
        return "sparse_devices";
    }
    return "unknown";
}

bool audit_report_ok(const ArchiveAuditReport& report) {
    if (!report.readable) {
        return false;
    }
    return std::none_of(
        report.findings.begin(),
        report.findings.end(),
        [](const AuditFinding& finding) {
            return finding.severity == AuditSeverity::error;
        });
}

bool fleet_audit_ok(const ArchiveFleetAuditReport& report) {
    return report.error_count == 0;
}

ArchiveAuditReport audit_archive_integrity(const std::filesystem::path& path,
                                           const AuditOptions& options) {
    ArchiveAuditReport report;
    report.path = path;
    try {
        ArchiveReader reader(path);
        auto header = reader.summary();
        bool has_previous = false;
        std::uint64_t previous_time = 0;
        std::map<protocol::DeviceId, std::uint32_t> last_sequence;
        std::set<std::uint64_t> offsets;
        while (auto record = reader.next()) {
            check_duplicate_offset(report, *record, offsets);
            if (options.require_time_order) {
                check_time_order(report, *record, has_previous, previous_time);
            }
            if (options.require_device_sequence_monotonicity) {
                check_device_sequence(report, *record, last_sequence);
            }
            check_payload_bounds(report, *record, options);
            if (report.records_scanned == 0) {
                report.first_offset = record->offset;
            }
            report.last_offset = record->offset;
            report.bytes_estimated += estimate_archive_record_bytes(*record);
            previous_time = record->capture_time_ns;
            has_previous = true;
            ++report.records_scanned;
        }
        report.manifest = build_archive_manifest(path);
        add_header_findings(report, header);
        add_manifest_findings(report);
        add_sparse_device_findings(report, options);
    } catch (const Error& error) {
        report.readable = false;
        report.findings.push_back(make_finding(
            AuditSeverity::error,
            AuditCheck::readable,
            path,
            error.what()));
    }
    return report;
}

ArchiveFleetAuditReport audit_archive_fleet(const std::vector<std::filesystem::path>& paths,
                                            const AuditOptions& options) {
    ArchiveFleetAuditReport fleet;
    for (const auto& path : paths) {
        auto report = audit_archive_integrity(path, options);
        accumulate_fleet_counts(fleet, report);
        fleet.archives.push_back(std::move(report));
    }
    std::map<protocol::DeviceId, std::filesystem::path> last_seen_device;
    for (const auto& archive : fleet.archives) {
        for (const auto& device : archive.manifest.devices) {
            auto it = last_seen_device.find(device.device);
            if (it != last_seen_device.end() && it->second != archive.path) {
                AuditFinding finding = make_finding(
                    AuditSeverity::info,
                    AuditCheck::sparse_devices,
                    archive.path,
                    "device appears in multiple archives");
                finding.device = device.device;
                fleet.fleet_findings.push_back(std::move(finding));
            }
            last_seen_device[device.device] = archive.path;
        }
    }
    return fleet;
}

std::vector<AuditFinding> findings_by_severity(const ArchiveAuditReport& report,
                                               AuditSeverity severity) {
    std::vector<AuditFinding> findings;
    for (const auto& finding : report.findings) {
        if (finding.severity == severity) {
            findings.push_back(finding);
        }
    }
    return findings;
}

std::vector<AuditFinding> fleet_findings_by_severity(const ArchiveFleetAuditReport& report,
                                                     AuditSeverity severity) {
    std::vector<AuditFinding> findings;
    for (const auto& archive : report.archives) {
        auto subset = findings_by_severity(archive, severity);
        findings.insert(findings.end(), subset.begin(), subset.end());
    }
    for (const auto& finding : report.fleet_findings) {
        if (finding.severity == severity) {
            findings.push_back(finding);
        }
    }
    return findings;
}

std::string render_audit_finding(const AuditFinding& finding) {
    std::ostringstream out;
    out << audit_severity_name(finding.severity)
        << " check=" << audit_check_name(finding.check)
        << " path=" << finding.path.string()
        << " offset=" << finding.offset
        << " capture_time_ns=" << finding.capture_time_ns
        << " device=" << finding.device
        << " sequence=" << finding.sequence
        << " message=\"" << finding.message << "\"";
    return out.str();
}

std::string render_archive_audit_report(const ArchiveAuditReport& report) {
    std::ostringstream out;
    out << "archive_audit\n"
        << "  path: " << report.path.string() << "\n"
        << "  readable: " << (report.readable ? "true" : "false") << "\n"
        << "  records_scanned: " << report.records_scanned << "\n"
        << "  bytes_estimated: " << report.bytes_estimated << "\n"
        << "  min_payload_size: " << report.min_payload_size << "\n"
        << "  max_payload_size: " << report.max_payload_size << "\n"
        << "  first_offset: " << report.first_offset << "\n"
        << "  last_offset: " << report.last_offset << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: " << render_audit_finding(finding) << "\n";
    }
    return out.str();
}

std::string render_archive_fleet_audit_report(const ArchiveFleetAuditReport& report) {
    std::ostringstream out;
    out << "archive_fleet_audit\n"
        << "  archives_scanned: " << report.archives_scanned << "\n"
        << "  records_scanned: " << report.records_scanned << "\n"
        << "  error_count: " << report.error_count << "\n"
        << "  warning_count: " << report.warning_count << "\n";
    for (const auto& archive : report.archives) {
        out << render_archive_audit_report(archive);
    }
    for (const auto& finding : report.fleet_findings) {
        out << "  fleet_finding: " << render_audit_finding(finding) << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
