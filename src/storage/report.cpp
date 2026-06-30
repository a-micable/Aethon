#include "aethon/storage/report.hpp"

#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

ArchiveReportSection make_section(std::string title, std::string body) {
    return ArchiveReportSection{
        std::move(title),
        std::move(body),
    };
}

std::string render_device_section(const ArchiveManifest& manifest) {
    std::ostringstream out;
    for (const auto& device : manifest.devices) {
        out << "device "
            << device.device
            << ": records="
            << device.record_count
            << " payload_bytes="
            << device.total_payload_bytes
            << " first="
            << device.first_time_ns
            << " last="
            << device.last_time_ns
            << "\n";
    }
    return out.str();
}

std::string render_kind_section(const ArchiveManifest& manifest) {
    std::ostringstream out;
    for (const auto& kind : manifest.packet_kinds) {
        out << "kind "
            << static_cast<unsigned>(kind.kind)
            << ": records="
            << kind.count
            << "\n";
    }
    return out.str();
}

} // namespace

ArchiveReport build_archive_report(const std::filesystem::path& path,
                                   const ArchiveReportOptions& options) {
    ArchiveReport report;
    report.path = path;
    report.manifest = build_archive_manifest(path);
    report.health = diagnostics::analyze_archive_health(report.manifest);

    if (options.include_manifest) {
        report.sections.push_back(make_section(
            "manifest",
            render_archive_manifest(report.manifest)));
    }
    if (options.include_health) {
        report.sections.push_back(make_section(
            "health",
            diagnostics::render_archive_health_report(report.health)));
    }
    if (options.include_devices) {
        report.sections.push_back(make_section(
            "devices",
            render_device_section(report.manifest)));
    }
    if (options.include_packet_kinds) {
        report.sections.push_back(make_section(
            "packet_kinds",
            render_kind_section(report.manifest)));
    }
    return report;
}

std::string render_archive_report(const ArchiveReport& report) {
    std::ostringstream out;
    out << "archive_report\n"
        << "  path: "
        << report.path.string()
        << "\n"
        << "  records: "
        << report.manifest.summary.record_count
        << "\n"
        << "  health_ok: "
        << (report.health.ok() ? "yes" : "no")
        << "\n";
    for (const auto& section : report.sections) {
        out << render_report_section(section);
    }
    return out.str();
}

std::string render_report_section(const ArchiveReportSection& section) {
    std::ostringstream out;
    out << "\n["
        << section.title
        << "]\n"
        << section.body;
    if (!section.body.empty() && section.body.back() != '\n') {
        out << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
