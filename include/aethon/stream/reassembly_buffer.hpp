#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct ReassemblyBufferSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ReassemblyBufferSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ReassemblyBuffer {
public:
    explicit ReassemblyBuffer(std::string name = "reassembly_buffer");
    void observe(ReassemblyBufferSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ReassemblyBufferSummary summarize() const;
    [[nodiscard]] std::optional<ReassemblyBufferSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ReassemblyBufferSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ReassemblyBufferSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ReassemblyBufferSummary summarize_reassembly_buffer(const std::vector<ReassemblyBufferSample>& samples);
double reassembly_buffer_stability_index(const ReassemblyBufferSummary& summary);
std::string describe_reassembly_buffer(const ReassemblyBufferSummary& summary);

} // namespace aethon::stream
