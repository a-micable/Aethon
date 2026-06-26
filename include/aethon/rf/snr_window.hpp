#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct SnrWindowEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SnrWindowDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SnrWindow {
public:
    explicit SnrWindow(std::string owner = "snr_window");
    void insert(SnrWindowEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SnrWindowDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SnrWindowEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SnrWindowEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SnrWindowEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SnrWindowDecision merge_snr_window_decisions(const std::vector<SnrWindowDecision>& decisions);
std::string render_snr_window_decision(const SnrWindowDecision& decision);
double snr_window_pressure(const SnrWindow& component);

} // namespace aethon::rf
