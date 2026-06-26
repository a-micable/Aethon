#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct DeviceLockEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct DeviceLockDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class DeviceLock {
public:
    explicit DeviceLock(std::string owner = "device_lock");
    void insert(DeviceLockEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] DeviceLockDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<DeviceLockEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<DeviceLockEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<DeviceLockEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

DeviceLockDecision merge_device_lock_decisions(const std::vector<DeviceLockDecision>& decisions);
std::string render_device_lock_decision(const DeviceLockDecision& decision);
double device_lock_pressure(const DeviceLock& component);

} // namespace aethon::control
