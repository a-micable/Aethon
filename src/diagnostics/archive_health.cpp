#include "aethon/diagnostics/archive_health.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

void add_finding(ArchiveHealthReport& report,
                 HealthSeverity severity,
                 std::string code,
                 std::string message) {
    report.findings.push_back(HealthFinding{
        severity,
        std::move(code),
        std::move(message),
    });
}

std::uint64_t payload_total(const storage::ArchiveManifest& manifest) {
    std::uint64_t total = 0;
    for (const auto& device : manifest.devices) {
        total += device.total_payload_bytes;
    }
    return total;
}

std::uint64_t largest_device_count(const storage::ArchiveManifest& manifest) {
    std::uint64_t largest = 0;
    for (const auto& device : manifest.devices) {
        largest = std::max(largest, device.record_count);
    }
    return largest;
}

void check_summary(ArchiveHealthReport& report,
                   const storage::ArchiveManifest& manifest,
                   const HealthRuleSet& rules) {
    if (manifest.summary.record_count < rules.minimum_records) {
        add_finding(
            report,
            HealthSeverity::error,
            "archive.too_small",
            "archive does not contain the minimum required number of records");
    }
    if (manifest.summary.record_count == 0) {
        add_finding(
            report,
            HealthSeverity::warning,
            "archive.empty",
            "archive has no readable records");
    }
    if (manifest.summary.first_time_ns > manifest.summary.last_time_ns) {
        add_finding(
            report,
            HealthSeverity::error,
            "archive.time_inverted",
            "archive summary has first timestamp after last timestamp");
    }
}

void check_devices(ArchiveHealthReport& report,
                   const storage::ArchiveManifest& manifest,
                   const HealthRuleSet& rules) {
    if (manifest.devices.size() > rules.maximum_devices) {
        add_finding(
            report,
            HealthSeverity::warning,
            "archive.too_many_devices",
            "archive contains more devices than the configured limit");
    }
    for (const auto& device : manifest.devices) {
        if (device.record_count == 0) {
            add_finding(
                report,
                HealthSeverity::warning,
                "device.empty",
                "device aggregate contains no records");
        }
        if (rules.require_monotonic_device_ranges && device.first_time_ns > device.last_time_ns) {
            add_finding(
                report,
                HealthSeverity::error,
                "device.time_inverted",
                "device aggregate has inverted timestamp range");
        }
    }
}

void check_distribution(ArchiveHealthReport& report,
                        const storage::ArchiveManifest& manifest,
                        const HealthRuleSet& rules) {
    if (manifest.summary.record_count == 0) {
        return;
    }
    auto largest = largest_device_count(manifest);
    auto ratio = static_cast<double>(largest) / static_cast<double>(manifest.summary.record_count);
    if (ratio > rules.maximum_single_device_ratio && manifest.devices.size() > 1) {
        add_finding(
            report,
            HealthSeverity::warning,
            "archive.device_skew",
            "one device dominates the archive record distribution");
    }
    if (report.average_payload_bytes < rules.minimum_average_payload_bytes) {
        add_finding(
            report,
            HealthSeverity::warning,
            "archive.low_payload_density",
            "average payload size is below the configured threshold");
    }
}

void copy_manifest_warnings(ArchiveHealthReport& report,
                            const storage::ArchiveManifest& manifest) {
    for (const auto& warning : manifest.warnings) {
        add_finding(
            report,
            HealthSeverity::warning,
            "manifest.warning",
            warning);
    }
}

} // namespace

bool ArchiveHealthReport::ok() const noexcept {
    return std::none_of(
        findings.begin(),
        findings.end(),
        [](const HealthFinding& finding) {
            return finding.severity == HealthSeverity::error;
        });
}

ArchiveHealthReport analyze_archive_health(const storage::ArchiveManifest& manifest,
                                           const HealthRuleSet& rules) {
    ArchiveHealthReport report;
    report.record_count = manifest.summary.record_count;
    report.device_count = manifest.devices.size();
    report.total_payload_bytes = payload_total(manifest);
    if (report.record_count != 0) {
        report.average_payload_bytes = static_cast<double>(report.total_payload_bytes)
            / static_cast<double>(report.record_count);
    }

    check_summary(report, manifest, rules);
    check_devices(report, manifest, rules);
    check_distribution(report, manifest, rules);
    copy_manifest_warnings(report, manifest);

    if (report.findings.empty()) {
        add_finding(
            report,
            HealthSeverity::info,
            "archive.ok",
            "archive health checks passed");
    }
    return report;
}

std::string health_severity_name(HealthSeverity severity) {
    switch (severity) {
    case HealthSeverity::info:
        return "info";
    case HealthSeverity::warning:
        return "warning";
    case HealthSeverity::error:
        return "error";
    }
    return "unknown";
}

std::string render_archive_health_report(const ArchiveHealthReport& report) {
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(2)
        << "archive_health\n"
        << "  ok: "
        << (report.ok() ? "yes" : "no")
        << "\n"
        << "  records: "
        << report.record_count
        << "\n"
        << "  devices: "
        << report.device_count
        << "\n"
        << "  payload_bytes: "
        << report.total_payload_bytes
        << "\n"
        << "  average_payload_bytes: "
        << report.average_payload_bytes
        << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: "
            << health_severity_name(finding.severity)
            << " "
            << finding.code
            << " "
            << finding.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::diagnostics
