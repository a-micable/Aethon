#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::sensors {

struct DeviceRegistrySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct DeviceRegistrySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class DeviceRegistry {
public:
    explicit DeviceRegistry(std::string name = "device_registry");
    void observe(DeviceRegistrySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] DeviceRegistrySummary summarize() const;
    [[nodiscard]] std::optional<DeviceRegistrySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<DeviceRegistrySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<DeviceRegistrySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

DeviceRegistrySummary summarize_device_registry(const std::vector<DeviceRegistrySample>& samples);
double device_registry_stability_index(const DeviceRegistrySummary& summary);
std::string describe_device_registry(const DeviceRegistrySummary& summary);

} // namespace aethon::sensors
