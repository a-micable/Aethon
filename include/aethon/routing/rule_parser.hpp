#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::routing {

struct RuleParserSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RuleParserSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RuleParser {
public:
    explicit RuleParser(std::string name = "rule_parser");
    void observe(RuleParserSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RuleParserSummary summarize() const;
    [[nodiscard]] std::optional<RuleParserSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RuleParserSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RuleParserSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RuleParserSummary summarize_rule_parser(const std::vector<RuleParserSample>& samples);
double rule_parser_stability_index(const RuleParserSummary& summary);
std::string describe_rule_parser(const RuleParserSummary& summary);

} // namespace aethon::routing
