#pragma once

#include "aethon/runtime/collector.hpp"
#include "aethon/stream/link_monitor.hpp"
#include "aethon/stream/session_tracker.hpp"

#include <string>
#include <vector>

namespace aethon::diagnostics {

struct SystemReport {
    runtime::CollectorStats collector;
    stream::SessionTrackerSnapshot sessions;
    stream::LinkMonitorReport link;
    std::vector<std::string> notes;
};

[[nodiscard]] SystemReport build_system_report(const runtime::CollectorRuntime& runtime);
[[nodiscard]] std::string render_system_report(const SystemReport& report);

} // namespace aethon::diagnostics
