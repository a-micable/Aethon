#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct CaptureLabelEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct CaptureLabelDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class CaptureLabel {
public:
    explicit CaptureLabel(std::string owner = "capture_label");
    void insert(CaptureLabelEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] CaptureLabelDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<CaptureLabelEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<CaptureLabelEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<CaptureLabelEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

CaptureLabelDecision merge_capture_label_decisions(const std::vector<CaptureLabelDecision>& decisions);
std::string render_capture_label_decision(const CaptureLabelDecision& decision);
double capture_label_pressure(const CaptureLabel& component);

} // namespace aethon::telemetry
