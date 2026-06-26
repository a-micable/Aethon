#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

struct SchemaValidatorSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SchemaValidatorSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SchemaValidator {
public:
    explicit SchemaValidator(std::string name = "schema_validator");
    void observe(SchemaValidatorSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SchemaValidatorSummary summarize() const;
    [[nodiscard]] std::optional<SchemaValidatorSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SchemaValidatorSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SchemaValidatorSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SchemaValidatorSummary summarize_schema_validator(const std::vector<SchemaValidatorSample>& samples);
double schema_validator_stability_index(const SchemaValidatorSummary& summary);
std::string describe_schema_validator(const SchemaValidatorSummary& summary);

} // namespace aethon::config
