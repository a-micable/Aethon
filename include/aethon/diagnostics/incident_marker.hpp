#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct IncidentMarkerSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct IncidentMarkerSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class IncidentMarker {
public:
    explicit IncidentMarker(std::string name = "incident_marker");
    void observe(IncidentMarkerSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] IncidentMarkerSummary summarize() const;
    [[nodiscard]] std::optional<IncidentMarkerSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<IncidentMarkerSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<IncidentMarkerSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

IncidentMarkerSummary summarize_incident_marker(const std::vector<IncidentMarkerSample>& samples);
double incident_marker_stability_index(const IncidentMarkerSummary& summary);
std::string describe_incident_marker(const IncidentMarkerSummary& summary);

} // namespace aethon::diagnostics
