#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::serialization {

struct SchemaRegistrySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SchemaRegistrySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SchemaRegistry {
public:
    explicit SchemaRegistry(std::string name = "schema_registry");
    void observe(SchemaRegistrySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SchemaRegistrySummary summarize() const;
    [[nodiscard]] std::optional<SchemaRegistrySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SchemaRegistrySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SchemaRegistrySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SchemaRegistrySummary summarize_schema_registry(const std::vector<SchemaRegistrySample>& samples);
double schema_registry_stability_index(const SchemaRegistrySummary& summary);
std::string describe_schema_registry(const SchemaRegistrySummary& summary);

} // namespace aethon::serialization
