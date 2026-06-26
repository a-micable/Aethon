#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct SegmentWriterSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SegmentWriterSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SegmentWriter {
public:
    explicit SegmentWriter(std::string name = "segment_writer");
    void observe(SegmentWriterSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SegmentWriterSummary summarize() const;
    [[nodiscard]] std::optional<SegmentWriterSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SegmentWriterSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SegmentWriterSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SegmentWriterSummary summarize_segment_writer(const std::vector<SegmentWriterSample>& samples);
double segment_writer_stability_index(const SegmentWriterSummary& summary);
std::string describe_segment_writer(const SegmentWriterSummary& summary);

} // namespace aethon::storage
