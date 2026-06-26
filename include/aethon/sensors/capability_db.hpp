#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::sensors {

struct CapabilityDbSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CapabilityDbSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CapabilityDb {
public:
    explicit CapabilityDb(std::string name = "capability_db");
    void observe(CapabilityDbSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CapabilityDbSummary summarize() const;
    [[nodiscard]] std::optional<CapabilityDbSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CapabilityDbSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CapabilityDbSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CapabilityDbSummary summarize_capability_db(const std::vector<CapabilityDbSample>& samples);
double capability_db_stability_index(const CapabilityDbSummary& summary);
std::string describe_capability_db(const CapabilityDbSummary& summary);

} // namespace aethon::sensors
