#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

struct RuntimeProfileSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RuntimeProfileSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RuntimeProfile {
public:
    explicit RuntimeProfile(std::string name = "profile");
    void observe(RuntimeProfileSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RuntimeProfileSummary summarize() const;
    [[nodiscard]] std::optional<RuntimeProfileSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RuntimeProfileSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RuntimeProfileSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RuntimeProfileSummary summarize_profile(const std::vector<RuntimeProfileSample>& samples);
double profile_stability_index(const RuntimeProfileSummary& summary);
std::string describe_profile(const RuntimeProfileSummary& summary);

} // namespace aethon::config
