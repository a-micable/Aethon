#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct ResultPageEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ResultPageDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ResultPage {
public:
    explicit ResultPage(std::string owner = "result_page");
    void insert(ResultPageEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ResultPageDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ResultPageEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ResultPageEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ResultPageEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ResultPageDecision merge_result_page_decisions(const std::vector<ResultPageDecision>& decisions);
std::string render_result_page_decision(const ResultPageDecision& decision);
double result_page_pressure(const ResultPage& component);

} // namespace aethon::query
