#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::serialization {

struct RecordBuilderSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RecordBuilderSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RecordBuilder {
public:
    explicit RecordBuilder(std::string name = "record_builder");
    void observe(RecordBuilderSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RecordBuilderSummary summarize() const;
    [[nodiscard]] std::optional<RecordBuilderSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RecordBuilderSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RecordBuilderSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RecordBuilderSummary summarize_record_builder(const std::vector<RecordBuilderSample>& samples);
double record_builder_stability_index(const RecordBuilderSummary& summary);
std::string describe_record_builder(const RecordBuilderSummary& summary);

} // namespace aethon::serialization
