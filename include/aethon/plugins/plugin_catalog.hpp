#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::plugins {

struct PluginCatalogSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct PluginCatalogSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class PluginCatalog {
public:
    explicit PluginCatalog(std::string name = "plugin_catalog");
    void observe(PluginCatalogSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] PluginCatalogSummary summarize() const;
    [[nodiscard]] std::optional<PluginCatalogSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<PluginCatalogSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<PluginCatalogSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

PluginCatalogSummary summarize_plugin_catalog(const std::vector<PluginCatalogSample>& samples);
double plugin_catalog_stability_index(const PluginCatalogSummary& summary);
std::string describe_plugin_catalog(const PluginCatalogSummary& summary);

} // namespace aethon::plugins
