#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::protocol {

struct MetadataViewSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct MetadataViewSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class MetadataView {
public:
    explicit MetadataView(std::string name = "metadata_view");
    void observe(MetadataViewSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] MetadataViewSummary summarize() const;
    [[nodiscard]] std::optional<MetadataViewSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<MetadataViewSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<MetadataViewSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

MetadataViewSummary summarize_metadata_view(const std::vector<MetadataViewSample>& samples);
double metadata_view_stability_index(const MetadataViewSummary& summary);
std::string describe_metadata_view(const MetadataViewSummary& summary);

} // namespace aethon::protocol
