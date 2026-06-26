#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::integrity {

struct VerificationPlanSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct VerificationPlanSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class VerificationPlan {
public:
    explicit VerificationPlan(std::string name = "verification_plan");
    void observe(VerificationPlanSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] VerificationPlanSummary summarize() const;
    [[nodiscard]] std::optional<VerificationPlanSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<VerificationPlanSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<VerificationPlanSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

VerificationPlanSummary summarize_verification_plan(const std::vector<VerificationPlanSample>& samples);
double verification_plan_stability_index(const VerificationPlanSummary& summary);
std::string describe_verification_plan(const VerificationPlanSummary& summary);

} // namespace aethon::integrity
