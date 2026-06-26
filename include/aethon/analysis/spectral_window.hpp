#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct SpectralWindowSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SpectralWindowSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SpectralWindow {
public:
    explicit SpectralWindow(std::string name = "spectral_window");
    void observe(SpectralWindowSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SpectralWindowSummary summarize() const;
    [[nodiscard]] std::optional<SpectralWindowSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SpectralWindowSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SpectralWindowSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SpectralWindowSummary summarize_spectral_window(const std::vector<SpectralWindowSample>& samples);
double spectral_window_stability_index(const SpectralWindowSummary& summary);
std::string describe_spectral_window(const SpectralWindowSummary& summary);

} // namespace aethon::analysis
