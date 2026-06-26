#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct ArchiveRepairSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ArchiveRepairSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ArchiveRepair {
public:
    explicit ArchiveRepair(std::string name = "repair");
    void observe(ArchiveRepairSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ArchiveRepairSummary summarize() const;
    [[nodiscard]] std::optional<ArchiveRepairSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ArchiveRepairSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ArchiveRepairSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ArchiveRepairSummary summarize_repair(const std::vector<ArchiveRepairSample>& samples);
double repair_stability_index(const ArchiveRepairSummary& summary);
std::string describe_repair(const ArchiveRepairSummary& summary);

} // namespace aethon::storage
