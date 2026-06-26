#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::serialization {

struct VarintCodecSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct VarintCodecSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class VarintCodec {
public:
    explicit VarintCodec(std::string name = "varint");
    void observe(VarintCodecSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] VarintCodecSummary summarize() const;
    [[nodiscard]] std::optional<VarintCodecSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<VarintCodecSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<VarintCodecSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

VarintCodecSummary summarize_varint(const std::vector<VarintCodecSample>& samples);
double varint_stability_index(const VarintCodecSummary& summary);
std::string describe_varint(const VarintCodecSummary& summary);

} // namespace aethon::serialization
