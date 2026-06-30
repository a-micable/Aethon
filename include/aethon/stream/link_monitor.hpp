#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::stream {

struct LinkMonitorOptions {
    std::uint64_t jitter_warning_ns = 100'000'000ULL;
    std::uint64_t idle_warning_ns = 5'000'000'000ULL;
};

struct LinkSample {
    protocol::DeviceId device = 0;
    std::uint64_t arrival_time_ns = 0;
    std::uint32_t sequence = 0;
};

struct LinkFinding {
    protocol::DeviceId device = 0;
    std::string code;
    std::string message;
};

struct LinkMonitorReport {
    std::uint64_t samples = 0;
    std::vector<LinkFinding> findings;
};

class LinkMonitor {
public:
    explicit LinkMonitor(LinkMonitorOptions options = {});

    void observe(const protocol::Packet& packet, std::uint64_t arrival_time_ns);
    [[nodiscard]] LinkMonitorReport report() const;
    void clear();

private:
    LinkMonitorOptions options_;
    std::vector<LinkSample> samples_;
};

[[nodiscard]] std::string render_link_monitor_report(const LinkMonitorReport& report);

} // namespace aethon::stream
