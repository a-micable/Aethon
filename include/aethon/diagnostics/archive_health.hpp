#pragma once

#include "aethon/storage/manifest.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::diagnostics {

enum class HealthSeverity {
    info,
    warning,
    error,
};

struct HealthRuleSet {
    std::uint64_t minimum_records = 1;
    std::uint64_t maximum_devices = 4096;
    double maximum_single_device_ratio = 0.95;
    double minimum_average_payload_bytes = 1.0;
    bool require_monotonic_device_ranges = true;
};

struct HealthFinding {
    HealthSeverity severity = HealthSeverity::info;
    std::string code;
    std::string message;
};

struct ArchiveHealthReport {
    std::uint64_t record_count = 0;
    std::uint64_t device_count = 0;
    std::uint64_t total_payload_bytes = 0;
    double average_payload_bytes = 0.0;
    std::vector<HealthFinding> findings;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] ArchiveHealthReport analyze_archive_health(const storage::ArchiveManifest& manifest,
                                                         const HealthRuleSet& rules = {});

[[nodiscard]] std::string health_severity_name(HealthSeverity severity);
[[nodiscard]] std::string render_archive_health_report(const ArchiveHealthReport& report);

} // namespace aethon::diagnostics
