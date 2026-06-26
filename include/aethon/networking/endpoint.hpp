#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::networking {

struct EndpointSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct EndpointSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class Endpoint {
public:
    explicit Endpoint(std::string name = "endpoint");
    void observe(EndpointSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] EndpointSummary summarize() const;
    [[nodiscard]] std::optional<EndpointSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<EndpointSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<EndpointSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

EndpointSummary summarize_endpoint(const std::vector<EndpointSample>& samples);
double endpoint_stability_index(const EndpointSummary& summary);
std::string describe_endpoint(const EndpointSummary& summary);

} // namespace aethon::networking
