#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct FirmwareRolloutEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct FirmwareRolloutDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class FirmwareRollout {
public:
    explicit FirmwareRollout(std::string owner = "firmware_rollout");
    void insert(FirmwareRolloutEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] FirmwareRolloutDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<FirmwareRolloutEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<FirmwareRolloutEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<FirmwareRolloutEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

FirmwareRolloutDecision merge_firmware_rollout_decisions(const std::vector<FirmwareRolloutDecision>& decisions);
std::string render_firmware_rollout_decision(const FirmwareRolloutDecision& decision);
double firmware_rollout_pressure(const FirmwareRollout& component);

} // namespace aethon::control
