#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct SpanSamplerEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SpanSamplerDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SpanSampler {
public:
    explicit SpanSampler(std::string owner = "span_sampler");
    void insert(SpanSamplerEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SpanSamplerDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SpanSamplerEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SpanSamplerEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SpanSamplerEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SpanSamplerDecision merge_span_sampler_decisions(const std::vector<SpanSamplerDecision>& decisions);
std::string render_span_sampler_decision(const SpanSamplerDecision& decision);
double span_sampler_pressure(const SpanSampler& component);

} // namespace aethon::observability
