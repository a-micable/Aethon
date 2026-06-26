#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct AccessWindowEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct AccessWindowDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class AccessWindow {
public:
    explicit AccessWindow(std::string owner = "access_window");
    void insert(AccessWindowEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] AccessWindowDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<AccessWindowEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<AccessWindowEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<AccessWindowEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

AccessWindowDecision merge_access_window_decisions(const std::vector<AccessWindowDecision>& decisions);
std::string render_access_window_decision(const AccessWindowDecision& decision);
double access_window_pressure(const AccessWindow& component);

} // namespace aethon::security
