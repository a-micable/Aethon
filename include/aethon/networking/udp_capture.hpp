#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::networking {

struct UdpCaptureSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct UdpCaptureSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class UdpCapture {
public:
    explicit UdpCapture(std::string name = "udp_capture");
    void observe(UdpCaptureSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] UdpCaptureSummary summarize() const;
    [[nodiscard]] std::optional<UdpCaptureSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<UdpCaptureSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<UdpCaptureSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

UdpCaptureSummary summarize_udp_capture(const std::vector<UdpCaptureSample>& samples);
double udp_capture_stability_index(const UdpCaptureSummary& summary);
std::string describe_udp_capture(const UdpCaptureSummary& summary);

} // namespace aethon::networking
