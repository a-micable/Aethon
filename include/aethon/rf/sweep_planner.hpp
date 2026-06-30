#pragma once

#include "aethon/rf/band_plan.hpp"
#include "aethon/rf/interference.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class SweepPriority : std::uint8_t {
    low = 0,
    normal = 1,
    high = 2,
    urgent = 3,
};

struct ReceiverProfile {
    std::string name;
    std::uint64_t min_frequency_hz = 0;
    std::uint64_t max_frequency_hz = 6'000'000'000ULL;
    std::uint64_t max_span_hz = 20'000'000ULL;
    std::uint64_t min_step_hz = 1'000ULL;
    std::uint64_t tuning_settle_us = 2'000;
    std::uint64_t max_sample_rate_hz = 20'000'000ULL;
    double usable_fraction = 0.8;
};

struct SweepRequest {
    SweepIntent intent = SweepIntent::survey;
    FrequencyRange range;
    std::optional<RfService> service;
    std::uint64_t resolution_hz = 25'000;
    std::uint64_t dwell_us = 20'000;
    std::uint64_t revisit_interval_us = 0;
    SweepPriority priority = SweepPriority::normal;
    bool include_guard_ranges = false;
    bool prefer_known_channels = true;
};

struct SweepSegment {
    FrequencyRange range;
    std::uint64_t center_frequency_hz = 0;
    std::uint64_t span_hz = 0;
    std::uint64_t bin_width_hz = 0;
    std::uint64_t dwell_us = 0;
    SweepPriority priority = SweepPriority::normal;
    SweepIntent intent = SweepIntent::survey;
    std::optional<RfChannel> channel;
    std::optional<BandAllocation> allocation;
    std::string reason;
};

struct SweepPlan {
    std::string name;
    std::vector<SweepSegment> segments;
    std::uint64_t estimated_duration_us = 0;
    std::uint64_t covered_hz = 0;
    double duty_cycle = 0.0;
    std::vector<std::string> warnings;
};

[[nodiscard]] SweepPlan plan_sweep(const BandPlan& band_plan,
                                   const ReceiverProfile& receiver,
                                   const std::vector<SweepRequest>& requests);
[[nodiscard]] SweepPlan plan_interference_followup(const BandPlan& band_plan,
                                                   const ReceiverProfile& receiver,
                                                   const InterferenceReport& report,
                                                   std::uint64_t resolution_hz = 5'000);
[[nodiscard]] std::vector<SweepSegment> segment_range(FrequencyRange range,
                                                      const ReceiverProfile& receiver,
                                                      std::uint64_t resolution_hz,
                                                      std::uint64_t dwell_us,
                                                      SweepPriority priority,
                                                      SweepIntent intent);
[[nodiscard]] std::vector<SweepSegment> channel_segments(const BandPlan& band_plan,
                                                         const ReceiverProfile& receiver,
                                                         const SweepRequest& request);
[[nodiscard]] SweepPlan optimize_sweep_plan(SweepPlan plan, const ReceiverProfile& receiver);
[[nodiscard]] std::uint64_t estimate_segment_duration_us(const SweepSegment& segment,
                                                         const ReceiverProfile& receiver);
[[nodiscard]] std::uint64_t estimate_plan_duration_us(const SweepPlan& plan,
                                                      const ReceiverProfile& receiver);
[[nodiscard]] double priority_weight(SweepPriority priority);
[[nodiscard]] std::string sweep_priority_name(SweepPriority priority);
[[nodiscard]] std::string sweep_intent_name(SweepIntent intent);
[[nodiscard]] std::string render_sweep_segment(const SweepSegment& segment);
[[nodiscard]] std::string render_sweep_plan(const SweepPlan& plan);

} // namespace aethon::rf
