#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct DeviceNoteEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct DeviceNoteDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class DeviceNote {
public:
    explicit DeviceNote(std::string owner = "device_note");
    void insert(DeviceNoteEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] DeviceNoteDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<DeviceNoteEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<DeviceNoteEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<DeviceNoteEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

DeviceNoteDecision merge_device_note_decisions(const std::vector<DeviceNoteDecision>& decisions);
std::string render_device_note_decision(const DeviceNoteDecision& decision);
double device_note_pressure(const DeviceNote& component);

} // namespace aethon::telemetry
