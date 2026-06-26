#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct MaintenanceWindowEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct MaintenanceWindowDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class MaintenanceWindow {
public:
    explicit MaintenanceWindow(std::string owner = "maintenance_window");
    void insert(MaintenanceWindowEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] MaintenanceWindowDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<MaintenanceWindowEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<MaintenanceWindowEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<MaintenanceWindowEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

MaintenanceWindowDecision merge_maintenance_window_decisions(const std::vector<MaintenanceWindowDecision>& decisions);
std::string render_maintenance_window_decision(const MaintenanceWindowDecision& decision);
double maintenance_window_pressure(const MaintenanceWindow& component);

} // namespace aethon::control
