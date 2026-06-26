#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct SloWindowEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SloWindowDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SloWindow {
public:
    explicit SloWindow(std::string owner = "slo_window");
    void insert(SloWindowEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SloWindowDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SloWindowEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SloWindowEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SloWindowEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SloWindowDecision merge_slo_window_decisions(const std::vector<SloWindowDecision>& decisions);
std::string render_slo_window_decision(const SloWindowDecision& decision);
double slo_window_pressure(const SloWindow& component);

} // namespace aethon::observability
