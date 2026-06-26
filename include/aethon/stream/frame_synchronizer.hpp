#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct FrameSynchronizerSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct FrameSynchronizerSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class FrameSynchronizer {
public:
    explicit FrameSynchronizer(std::string name = "frame_synchronizer");
    void observe(FrameSynchronizerSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] FrameSynchronizerSummary summarize() const;
    [[nodiscard]] std::optional<FrameSynchronizerSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<FrameSynchronizerSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<FrameSynchronizerSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

FrameSynchronizerSummary summarize_frame_synchronizer(const std::vector<FrameSynchronizerSample>& samples);
double frame_synchronizer_stability_index(const FrameSynchronizerSummary& summary);
std::string describe_frame_synchronizer(const FrameSynchronizerSummary& summary);

} // namespace aethon::stream
