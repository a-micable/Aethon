#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct DashboardHintEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct DashboardHintDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class DashboardHint {
public:
    explicit DashboardHint(std::string owner = "dashboard_hint");
    void insert(DashboardHintEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] DashboardHintDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<DashboardHintEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<DashboardHintEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<DashboardHintEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

DashboardHintDecision merge_dashboard_hint_decisions(const std::vector<DashboardHintDecision>& decisions);
std::string render_dashboard_hint_decision(const DashboardHintDecision& decision);
double dashboard_hint_pressure(const DashboardHint& component);

} // namespace aethon::observability
