#include "aethon/stream/link_monitor.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

namespace aethon::stream {
namespace {

void add_finding(LinkMonitorReport& report,
                 protocol::DeviceId device,
                 std::string code,
                 std::string message) {
    report.findings.push_back(LinkFinding{
        device,
        std::move(code),
        std::move(message),
    });
}

std::map<protocol::DeviceId, std::vector<LinkSample>> group_by_device(const std::vector<LinkSample>& samples) {
    std::map<protocol::DeviceId, std::vector<LinkSample>> grouped;
    for (const auto& sample : samples) {
        grouped[sample.device].push_back(sample);
    }
    return grouped;
}

} // namespace

LinkMonitor::LinkMonitor(LinkMonitorOptions options)
    : options_(options) {}

void LinkMonitor::observe(const protocol::Packet& packet, std::uint64_t arrival_time_ns) {
    samples_.push_back(LinkSample{
        packet.device,
        arrival_time_ns,
        packet.sequence,
    });
}

LinkMonitorReport LinkMonitor::report() const {
    LinkMonitorReport report;
    report.samples = samples_.size();
    auto grouped = group_by_device(samples_);
    for (auto& [device, samples] : grouped) {
        std::sort(
            samples.begin(),
            samples.end(),
            [](const LinkSample& left, const LinkSample& right) {
                return left.arrival_time_ns < right.arrival_time_ns;
            });
        for (std::size_t i = 1; i < samples.size(); ++i) {
            auto delta = samples[i].arrival_time_ns - samples[i - 1].arrival_time_ns;
            if (delta > options_.idle_warning_ns) {
                add_finding(report, device, "link.idle_gap", "device had a long idle gap");
            } else if (delta > options_.jitter_warning_ns) {
                add_finding(report, device, "link.jitter", "device arrival delta exceeded jitter threshold");
            }
            if (samples[i].sequence <= samples[i - 1].sequence) {
                add_finding(report, device, "link.sequence_regression", "sequence did not advance");
            }
        }
    }
    return report;
}

void LinkMonitor::clear() {
    samples_.clear();
}

std::string render_link_monitor_report(const LinkMonitorReport& report) {
    std::ostringstream out;
    out << "link_monitor\n"
        << "  samples: "
        << report.samples
        << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: device="
            << finding.device
            << " "
            << finding.code
            << " "
            << finding.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::stream
