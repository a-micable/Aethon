#include "aethon/diagnostics/system_report.hpp"

#include <sstream>

namespace aethon::diagnostics {

SystemReport build_system_report(const runtime::CollectorRuntime& runtime) {
    SystemReport report;
    report.collector = runtime.stats();
    report.sessions = runtime.sessions();
    report.link = runtime.link_report();
    if (report.collector.packets_rejected != 0) {
        report.notes.push_back("collector rejected one or more packets");
    }
    if (report.link.findings.empty()) {
        report.notes.push_back("link monitor has no findings");
    }
    return report;
}

std::string render_system_report(const SystemReport& report) {
    std::ostringstream out;
    out << "system_report\n"
        << runtime::render_collector_stats(report.collector)
        << stream::render_session_snapshot(report.sessions)
        << stream::render_link_monitor_report(report.link);
    for (const auto& note : report.notes) {
        out << "  note: "
            << note
            << "\n";
    }
    return out.str();
}

} // namespace aethon::diagnostics
