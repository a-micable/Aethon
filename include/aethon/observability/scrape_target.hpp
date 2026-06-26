#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct ScrapeTargetEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ScrapeTargetDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ScrapeTarget {
public:
    explicit ScrapeTarget(std::string owner = "scrape_target");
    void insert(ScrapeTargetEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ScrapeTargetDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ScrapeTargetEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ScrapeTargetEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ScrapeTargetEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ScrapeTargetDecision merge_scrape_target_decisions(const std::vector<ScrapeTargetDecision>& decisions);
std::string render_scrape_target_decision(const ScrapeTargetDecision& decision);
double scrape_target_pressure(const ScrapeTarget& component);

} // namespace aethon::observability
