#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::networking {

struct LinkBudgetSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct LinkBudgetSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class LinkBudget {
public:
    explicit LinkBudget(std::string name = "link_budget");
    void observe(LinkBudgetSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] LinkBudgetSummary summarize() const;
    [[nodiscard]] std::optional<LinkBudgetSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<LinkBudgetSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<LinkBudgetSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

LinkBudgetSummary summarize_link_budget(const std::vector<LinkBudgetSample>& samples);
double link_budget_stability_index(const LinkBudgetSummary& summary);
std::string describe_link_budget(const LinkBudgetSummary& summary);

} // namespace aethon::networking
