#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::runtime {

enum class DeviceLifecycle {
    unknown,
    provisioned,
    active,
    quarantined,
    retired,
};

struct DeviceOwner {
    std::string team;
    std::string contact;
    std::string site;
};

struct DeviceRecord {
    protocol::DeviceId device = 0;
    std::string serial;
    DeviceLifecycle lifecycle = DeviceLifecycle::unknown;
    DeviceOwner owner;
    protocol::DeviceCapabilities capabilities;
    std::uint64_t first_seen_ns = 0;
    std::uint64_t last_seen_ns = 0;
    std::uint64_t packets_seen = 0;
    std::vector<std::string> labels;
};

struct FleetSummary {
    std::uint64_t total_devices = 0;
    std::uint64_t active_devices = 0;
    std::uint64_t quarantined_devices = 0;
    std::uint64_t retired_devices = 0;
    std::uint64_t packets_seen = 0;
};

class FleetRegistry {
public:
    void upsert(DeviceRecord record);
    [[nodiscard]] bool erase(protocol::DeviceId device);
    [[nodiscard]] std::optional<DeviceRecord> find(protocol::DeviceId device) const;

    void mark_seen(const protocol::Packet& packet, std::uint64_t now_ns);
    void set_lifecycle(protocol::DeviceId device, DeviceLifecycle lifecycle);
    void add_label(protocol::DeviceId device, std::string label);

    [[nodiscard]] std::vector<DeviceRecord> by_lifecycle(DeviceLifecycle lifecycle) const;
    [[nodiscard]] std::vector<DeviceRecord> by_site(const std::string& site) const;
    [[nodiscard]] FleetSummary summary() const;
    [[nodiscard]] const std::map<protocol::DeviceId, DeviceRecord>& records() const noexcept;

private:
    std::map<protocol::DeviceId, DeviceRecord> records_;
};

[[nodiscard]] std::string lifecycle_name(DeviceLifecycle lifecycle);
[[nodiscard]] std::string render_device_record(const DeviceRecord& record);
[[nodiscard]] std::string render_fleet_summary(const FleetSummary& summary);

} // namespace aethon::runtime
