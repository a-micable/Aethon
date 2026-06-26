#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct ArchiveIndexSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ArchiveIndexSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ArchiveIndex {
public:
    explicit ArchiveIndex(std::string name = "archive_index");
    void observe(ArchiveIndexSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ArchiveIndexSummary summarize() const;
    [[nodiscard]] std::optional<ArchiveIndexSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ArchiveIndexSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ArchiveIndexSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ArchiveIndexSummary summarize_archive_index(const std::vector<ArchiveIndexSample>& samples);
double archive_index_stability_index(const ArchiveIndexSummary& summary);
std::string describe_archive_index(const ArchiveIndexSummary& summary);

} // namespace aethon::storage
