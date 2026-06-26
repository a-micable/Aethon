#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

struct ConfigLoaderSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ConfigLoaderSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ConfigLoader {
public:
    explicit ConfigLoader(std::string name = "loader");
    void observe(ConfigLoaderSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ConfigLoaderSummary summarize() const;
    [[nodiscard]] std::optional<ConfigLoaderSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ConfigLoaderSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ConfigLoaderSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ConfigLoaderSummary summarize_loader(const std::vector<ConfigLoaderSample>& samples);
double loader_stability_index(const ConfigLoaderSummary& summary);
std::string describe_loader(const ConfigLoaderSummary& summary);

} // namespace aethon::config
