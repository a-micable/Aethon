#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct JitterBufferSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct JitterBufferSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class JitterBuffer {
public:
    explicit JitterBuffer(std::string name = "jitter_buffer");
    void observe(JitterBufferSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] JitterBufferSummary summarize() const;
    [[nodiscard]] std::optional<JitterBufferSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<JitterBufferSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<JitterBufferSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

JitterBufferSummary summarize_jitter_buffer(const std::vector<JitterBufferSample>& samples);
double jitter_buffer_stability_index(const JitterBufferSummary& summary);
std::string describe_jitter_buffer(const JitterBufferSummary& summary);

} // namespace aethon::stream
