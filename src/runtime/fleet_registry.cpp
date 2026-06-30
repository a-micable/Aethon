#include "aethon/runtime/fleet_registry.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::runtime {
namespace {

bool has_label(const DeviceRecord& record, const std::string& label) {
    return std::find(record.labels.begin(), record.labels.end(), label) != record.labels.end();
}

void update_seen_fields(DeviceRecord& record, std::uint64_t now_ns) {
    if (record.first_seen_ns == 0) {
        record.first_seen_ns = now_ns;
    }
    record.last_seen_ns = now_ns;
    ++record.packets_seen;
}

void accumulate(FleetSummary& summary, const DeviceRecord& record) {
    ++summary.total_devices;
    summary.packets_seen += record.packets_seen;
    switch (record.lifecycle) {
    case DeviceLifecycle::active:
        ++summary.active_devices;
        break;
    case DeviceLifecycle::quarantined:
        ++summary.quarantined_devices;
        break;
    case DeviceLifecycle::retired:
        ++summary.retired_devices;
        break;
    case DeviceLifecycle::unknown:
    case DeviceLifecycle::provisioned:
        break;
    }
}

} // namespace

void FleetRegistry::upsert(DeviceRecord record) {
    records_[record.device] = std::move(record);
}

bool FleetRegistry::erase(protocol::DeviceId device) {
    return records_.erase(device) != 0;
}

std::optional<DeviceRecord> FleetRegistry::find(protocol::DeviceId device) const {
    auto it = records_.find(device);
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void FleetRegistry::mark_seen(const protocol::Packet& packet, std::uint64_t now_ns) {
    auto& record = records_[packet.device];
    if (record.device == 0) {
        record.device = packet.device;
        record.lifecycle = DeviceLifecycle::active;
    }
    if (packet.capabilities) {
        record.capabilities = *packet.capabilities;
    }
    update_seen_fields(record, now_ns);
}

void FleetRegistry::set_lifecycle(protocol::DeviceId device, DeviceLifecycle lifecycle) {
    auto& record = records_[device];
    if (record.device == 0) {
        record.device = device;
    }
    record.lifecycle = lifecycle;
}

void FleetRegistry::add_label(protocol::DeviceId device, std::string label) {
    auto& record = records_[device];
    if (record.device == 0) {
        record.device = device;
    }
    if (!has_label(record, label)) {
        record.labels.push_back(std::move(label));
    }
}

std::vector<DeviceRecord> FleetRegistry::by_lifecycle(DeviceLifecycle lifecycle) const {
    std::vector<DeviceRecord> out;
    for (const auto& [device, record] : records_) {
        (void)device;
        if (record.lifecycle == lifecycle) {
            out.push_back(record);
        }
    }
    return out;
}

std::vector<DeviceRecord> FleetRegistry::by_site(const std::string& site) const {
    std::vector<DeviceRecord> out;
    for (const auto& [device, record] : records_) {
        (void)device;
        if (record.owner.site == site) {
            out.push_back(record);
        }
    }
    return out;
}

FleetSummary FleetRegistry::summary() const {
    FleetSummary summary;
    for (const auto& [device, record] : records_) {
        (void)device;
        accumulate(summary, record);
    }
    return summary;
}

const std::map<protocol::DeviceId, DeviceRecord>& FleetRegistry::records() const noexcept {
    return records_;
}

std::string lifecycle_name(DeviceLifecycle lifecycle) {
    switch (lifecycle) {
    case DeviceLifecycle::unknown:
        return "unknown";
    case DeviceLifecycle::provisioned:
        return "provisioned";
    case DeviceLifecycle::active:
        return "active";
    case DeviceLifecycle::quarantined:
        return "quarantined";
    case DeviceLifecycle::retired:
        return "retired";
    }
    return "unknown";
}

std::string render_device_record(const DeviceRecord& record) {
    std::ostringstream out;
    out << "device "
        << record.device
        << "\n"
        << "  serial: "
        << record.serial
        << "\n"
        << "  lifecycle: "
        << lifecycle_name(record.lifecycle)
        << "\n"
        << "  site: "
        << record.owner.site
        << "\n"
        << "  packets_seen: "
        << record.packets_seen
        << "\n"
        << "  labels:";
    for (const auto& label : record.labels) {
        out << " " << label;
    }
    out << "\n";
    return out.str();
}

std::string render_fleet_summary(const FleetSummary& summary) {
    std::ostringstream out;
    out << "fleet_summary\n"
        << "  total_devices: "
        << summary.total_devices
        << "\n"
        << "  active_devices: "
        << summary.active_devices
        << "\n"
        << "  quarantined_devices: "
        << summary.quarantined_devices
        << "\n"
        << "  retired_devices: "
        << summary.retired_devices
        << "\n"
        << "  packets_seen: "
        << summary.packets_seen
        << "\n";
    return out.str();
}

} // namespace aethon::runtime
