#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::protocol {

struct ExtensionRegistrySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ExtensionRegistrySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ExtensionRegistry {
public:
    explicit ExtensionRegistry(std::string name = "extension_registry");
    void observe(ExtensionRegistrySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ExtensionRegistrySummary summarize() const;
    [[nodiscard]] std::optional<ExtensionRegistrySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ExtensionRegistrySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ExtensionRegistrySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ExtensionRegistrySummary summarize_extension_registry(const std::vector<ExtensionRegistrySample>& samples);
double extension_registry_stability_index(const ExtensionRegistrySummary& summary);
std::string describe_extension_registry(const ExtensionRegistrySummary& summary);

} // namespace aethon::protocol
