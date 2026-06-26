#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct IngestWindowSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct IngestWindowSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class IngestWindow {
public:
    explicit IngestWindow(std::string name = "ingest_window");
    void observe(IngestWindowSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] IngestWindowSummary summarize() const;
    [[nodiscard]] std::optional<IngestWindowSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<IngestWindowSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<IngestWindowSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

IngestWindowSummary summarize_ingest_window(const std::vector<IngestWindowSample>& samples);
double ingest_window_stability_index(const IngestWindowSummary& summary);
std::string describe_ingest_window(const IngestWindowSummary& summary);

} // namespace aethon::stream
