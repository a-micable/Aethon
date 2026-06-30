#pragma once

#include "aethon/rf/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aethon::rf {

struct IqSummaryConfig {
    double clipping_threshold = 0.98;
    double zero_crossing_deadband = 0.0;
    bool remove_dc_for_offset_estimate = false;
};

struct IqSegmentSummary {
    std::size_t first_sample = 0;
    std::size_t sample_count = 0;
    IqSummary summary;
};

[[nodiscard]] IqSummary summarize_iq(const IqWindow& window, const IqSummaryConfig& config = {});
[[nodiscard]] std::vector<IqSegmentSummary> summarize_iq_segments(const IqWindow& window,
                                                                  std::size_t segment_size,
                                                                  const IqSummaryConfig& config = {});
[[nodiscard]] IqWindow remove_dc(const IqWindow& window);
[[nodiscard]] IqWindow normalize_iq(const IqWindow& window, double target_rms = 0.5);
[[nodiscard]] double estimate_frequency_offset_hz(const IqWindow& window);
[[nodiscard]] double estimate_iq_phase_error_degrees(const IqWindow& window);
[[nodiscard]] double estimate_iq_gain_imbalance_db(const IqWindow& window);
[[nodiscard]] double magnitude(const IqSample& sample);
[[nodiscard]] double phase_radians(const IqSample& sample);
[[nodiscard]] std::string render_iq_summary(const IqSummary& summary);
[[nodiscard]] std::string render_iq_segment_summaries(const std::vector<IqSegmentSummary>& summaries);

} // namespace aethon::rf
