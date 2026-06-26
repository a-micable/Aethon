#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct ArchiveCompactionSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ArchiveCompactionSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ArchiveCompaction {
public:
    explicit ArchiveCompaction(std::string name = "compaction");
    void observe(ArchiveCompactionSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ArchiveCompactionSummary summarize() const;
    [[nodiscard]] std::optional<ArchiveCompactionSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ArchiveCompactionSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ArchiveCompactionSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ArchiveCompactionSummary summarize_compaction(const std::vector<ArchiveCompactionSample>& samples);
double compaction_stability_index(const ArchiveCompactionSummary& summary);
std::string describe_compaction(const ArchiveCompactionSummary& summary);

} // namespace aethon::storage
