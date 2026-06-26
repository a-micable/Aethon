#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::serialization {

struct FieldTableSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct FieldTableSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class FieldTable {
public:
    explicit FieldTable(std::string name = "field_table");
    void observe(FieldTableSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] FieldTableSummary summarize() const;
    [[nodiscard]] std::optional<FieldTableSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<FieldTableSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<FieldTableSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

FieldTableSummary summarize_field_table(const std::vector<FieldTableSample>& samples);
double field_table_stability_index(const FieldTableSummary& summary);
std::string describe_field_table(const FieldTableSummary& summary);

} // namespace aethon::serialization
