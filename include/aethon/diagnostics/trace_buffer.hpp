#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct TraceBufferSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct TraceBufferSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class TraceBuffer {
public:
    explicit TraceBuffer(std::string name = "trace_buffer");
    void observe(TraceBufferSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] TraceBufferSummary summarize() const;
    [[nodiscard]] std::optional<TraceBufferSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<TraceBufferSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<TraceBufferSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

TraceBufferSummary summarize_trace_buffer(const std::vector<TraceBufferSample>& samples);
double trace_buffer_stability_index(const TraceBufferSummary& summary);
std::string describe_trace_buffer(const TraceBufferSummary& summary);

} // namespace aethon::diagnostics
