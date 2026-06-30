#include "aethon/rf/sweep_planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

FrequencyRange receiver_clip(FrequencyRange range, const ReceiverProfile& receiver) {
    return {
        std::max(range.lower_hz, receiver.min_frequency_hz),
        std::min(range.upper_hz, receiver.max_frequency_hz),
    };
}

std::uint64_t effective_span(const ReceiverProfile& receiver) {
    auto fraction = std::clamp(receiver.usable_fraction, 0.1, 1.0);
    auto span = static_cast<std::uint64_t>(std::floor(static_cast<double>(receiver.max_span_hz) * fraction));
    return std::max<std::uint64_t>(span, receiver.min_step_hz);
}

std::uint64_t align_up(std::uint64_t value, std::uint64_t step) {
    if (step == 0) {
        return value;
    }
    auto remainder = value % step;
    if (remainder == 0) {
        return value;
    }
    return value + step - remainder;
}

std::uint64_t safe_add(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return left + right;
}

bool can_merge(const SweepSegment& left, const SweepSegment& right, const ReceiverProfile& receiver) {
    if (left.priority != right.priority || left.intent != right.intent) {
        return false;
    }
    if (left.allocation && right.allocation && left.allocation->name != right.allocation->name) {
        return false;
    }
    if (left.range.upper_hz < right.range.lower_hz) {
        auto gap = right.range.lower_hz - left.range.upper_hz;
        if (gap > left.bin_width_hz * 2) {
            return false;
        }
    }
    return merge(left.range, right.range).width_hz() <= effective_span(receiver);
}

SweepSegment merged_segment(const SweepSegment& left, const SweepSegment& right) {
    SweepSegment segment = left;
    segment.range = merge(left.range, right.range);
    segment.center_frequency_hz = segment.range.center_hz();
    segment.span_hz = segment.range.width_hz();
    segment.dwell_us = std::max(left.dwell_us, right.dwell_us);
    if (!segment.reason.empty()) {
        segment.reason += "; ";
    }
    segment.reason += right.reason;
    return segment;
}

void add_warning(SweepPlan& plan, std::string warning) {
    plan.warnings.push_back(std::move(warning));
}

FrequencyRange channel_range(const RfChannel& channel) {
    auto half = channel.bandwidth_hz / 2;
    return {
        channel.center_hz > half ? channel.center_hz - half : 0,
        channel.center_hz + half + (channel.bandwidth_hz % 2),
    };
}

SweepSegment make_segment(FrequencyRange range,
                          std::uint64_t bin_width_hz,
                          std::uint64_t dwell_us,
                          SweepPriority priority,
                          SweepIntent intent,
                          std::string reason) {
    SweepSegment segment;
    segment.range = range;
    segment.center_frequency_hz = range.center_hz();
    segment.span_hz = range.width_hz();
    segment.bin_width_hz = bin_width_hz;
    segment.dwell_us = dwell_us;
    segment.priority = priority;
    segment.intent = intent;
    segment.reason = std::move(reason);
    return segment;
}

std::uint64_t occupied_width(const std::vector<SweepSegment>& segments) {
    std::vector<FrequencyRange> ranges;
    ranges.reserve(segments.size());
    for (const auto& segment : segments) {
        ranges.push_back(segment.range);
    }
    std::sort(ranges.begin(), ranges.end(), [](const auto& left, const auto& right) {
        return left.lower_hz < right.lower_hz;
    });
    std::uint64_t total = 0;
    FrequencyRange current;
    for (const auto& range : ranges) {
        if (range.empty()) {
            continue;
        }
        if (current.empty()) {
            current = range;
            continue;
        }
        if (current.overlaps(range) || current.upper_hz == range.lower_hz) {
            current = merge(current, range);
            continue;
        }
        total = safe_add(total, current.width_hz());
        current = range;
    }
    if (!current.empty()) {
        total = safe_add(total, current.width_hz());
    }
    return total;
}

} // namespace

SweepPlan plan_sweep(const BandPlan& band_plan,
                     const ReceiverProfile& receiver,
                     const std::vector<SweepRequest>& requests) {
    SweepPlan plan;
    plan.name = receiver.name.empty() ? "rf_sweep" : receiver.name + " rf_sweep";
    for (const auto& request : requests) {
        if (request.range.empty()) {
            add_warning(plan, "ignored empty sweep request");
            continue;
        }
        auto clipped = receiver_clip(request.range, receiver);
        if (clipped.empty()) {
            add_warning(plan, "request outside receiver tuning range: " + format_range(request.range));
            continue;
        }

        std::vector<SweepSegment> request_segments;
        if (request.prefer_known_channels) {
            request_segments = channel_segments(band_plan, receiver, request);
        }
        if (request_segments.empty()) {
            request_segments = segment_range(
                clipped,
                receiver,
                request.resolution_hz,
                request.dwell_us,
                request.priority,
                request.intent);
        }

        for (auto& segment : request_segments) {
            segment.range = receiver_clip(segment.range, receiver);
            if (segment.range.empty()) {
                continue;
            }
            plan.segments.push_back(std::move(segment));
        }

        if (request.include_guard_ranges) {
            BandPlanQuery query;
            query.range = clipped;
            query.service = request.service;
            for (const auto& allocation : band_plan.query(query)) {
                for (const auto& guard : band_plan.guard_ranges(allocation)) {
                    auto clipped_guard = intersect(guard, clipped);
                    if (!clipped_guard) {
                        continue;
                    }
                    auto guard_segments = segment_range(*clipped_guard, receiver, request.resolution_hz, request.dwell_us, SweepPriority::high, request.intent);
                    for (auto& segment : guard_segments) {
                        segment.allocation = allocation;
                        segment.reason = "guard range for " + allocation.name;
                        plan.segments.push_back(std::move(segment));
                    }
                }
            }
        }
    }
    return optimize_sweep_plan(std::move(plan), receiver);
}

SweepPlan plan_interference_followup(const BandPlan& band_plan,
                                     const ReceiverProfile& receiver,
                                     const InterferenceReport& report,
                                     std::uint64_t resolution_hz) {
    std::vector<SweepRequest> requests;
    for (const auto& finding : report.findings) {
        SweepRequest request;
        request.intent = SweepIntent::interference_hunt;
        request.range = finding.range;
        request.resolution_hz = resolution_hz;
        request.dwell_us = finding.severity == InterferenceSeverity::critical ? 100'000 : 50'000;
        request.priority = finding.severity == InterferenceSeverity::critical ? SweepPriority::urgent : SweepPriority::high;
        request.include_guard_ranges = true;
        request.prefer_known_channels = false;
        if (finding.allocation) {
            request.service = finding.allocation->service;
            auto expanded = finding.range;
            auto pad = std::max<std::uint64_t>(finding.range.width_hz(), resolution_hz * 10);
            expanded.lower_hz = expanded.lower_hz > pad ? expanded.lower_hz - pad : 0;
            expanded.upper_hz = safe_add(expanded.upper_hz, pad);
            request.range = expanded;
        }
        requests.push_back(request);
    }
    auto plan = plan_sweep(band_plan, receiver, requests);
    plan.name = "interference_followup";
    return plan;
}

std::vector<SweepSegment> segment_range(FrequencyRange range,
                                        const ReceiverProfile& receiver,
                                        std::uint64_t resolution_hz,
                                        std::uint64_t dwell_us,
                                        SweepPriority priority,
                                        SweepIntent intent) {
    std::vector<SweepSegment> segments;
    range = receiver_clip(range, receiver);
    if (range.empty()) {
        return segments;
    }
    auto span = effective_span(receiver);
    auto bin_width = std::max<std::uint64_t>(resolution_hz, receiver.min_step_hz);
    auto cursor = range.lower_hz;
    while (cursor < range.upper_hz) {
        auto upper = std::min(range.upper_hz, safe_add(cursor, span));
        auto aligned_upper = upper < range.upper_hz ? align_up(upper, bin_width) : upper;
        aligned_upper = std::min(aligned_upper, range.upper_hz);
        auto segment = make_segment({cursor, aligned_upper}, bin_width, dwell_us, priority, intent, "range survey");
        segments.push_back(segment);
        if (aligned_upper <= cursor) {
            break;
        }
        cursor = aligned_upper;
    }
    return segments;
}

std::vector<SweepSegment> channel_segments(const BandPlan& band_plan,
                                           const ReceiverProfile& receiver,
                                           const SweepRequest& request) {
    std::vector<SweepSegment> segments;
    BandPlanQuery query;
    query.range = request.range;
    query.service = request.service;
    auto allocations = band_plan.query(query);
    for (const auto& allocation : allocations) {
        auto channels = band_plan.channels_for(allocation);
        for (const auto& channel : channels) {
            auto range = channel_range(channel);
            if (!range.overlaps(request.range)) {
                continue;
            }
            range = receiver_clip(range, receiver);
            if (range.empty()) {
                continue;
            }
            auto segment = make_segment(
                range,
                std::max<std::uint64_t>(request.resolution_hz, receiver.min_step_hz),
                request.dwell_us,
                request.priority,
                request.intent,
                "known channel " + channel.label);
            segment.channel = channel;
            segment.allocation = allocation;
            segments.push_back(std::move(segment));
        }
    }
    return segments;
}

SweepPlan optimize_sweep_plan(SweepPlan plan, const ReceiverProfile& receiver) {
    std::sort(plan.segments.begin(), plan.segments.end(), [](const auto& left, const auto& right) {
        if (priority_weight(left.priority) != priority_weight(right.priority)) {
            return priority_weight(left.priority) > priority_weight(right.priority);
        }
        return left.range.lower_hz < right.range.lower_hz;
    });

    std::vector<SweepSegment> merged;
    for (const auto& segment : plan.segments) {
        if (segment.range.empty()) {
            continue;
        }
        if (!merged.empty() && can_merge(merged.back(), segment, receiver)) {
            merged.back() = merged_segment(merged.back(), segment);
            continue;
        }
        merged.push_back(segment);
    }
    plan.segments = std::move(merged);
    plan.estimated_duration_us = estimate_plan_duration_us(plan, receiver);
    plan.covered_hz = occupied_width(plan.segments);
    if (plan.estimated_duration_us > 0) {
        std::uint64_t active = 0;
        for (const auto& segment : plan.segments) {
            active = safe_add(active, segment.dwell_us);
        }
        plan.duty_cycle = static_cast<double>(active) / static_cast<double>(plan.estimated_duration_us);
    }
    return plan;
}

std::uint64_t estimate_segment_duration_us(const SweepSegment& segment, const ReceiverProfile& receiver) {
    return safe_add(segment.dwell_us, receiver.tuning_settle_us);
}

std::uint64_t estimate_plan_duration_us(const SweepPlan& plan, const ReceiverProfile& receiver) {
    std::uint64_t total = 0;
    for (const auto& segment : plan.segments) {
        total = safe_add(total, estimate_segment_duration_us(segment, receiver));
    }
    return total;
}

double priority_weight(SweepPriority priority) {
    switch (priority) {
    case SweepPriority::low:
        return 0.5;
    case SweepPriority::normal:
        return 1.0;
    case SweepPriority::high:
        return 2.0;
    case SweepPriority::urgent:
        return 4.0;
    }
    return 1.0;
}

std::string sweep_priority_name(SweepPriority priority) {
    switch (priority) {
    case SweepPriority::low:
        return "low";
    case SweepPriority::normal:
        return "normal";
    case SweepPriority::high:
        return "high";
    case SweepPriority::urgent:
        return "urgent";
    }
    return "normal";
}

std::string sweep_intent_name(SweepIntent intent) {
    switch (intent) {
    case SweepIntent::survey:
        return "survey";
    case SweepIntent::occupancy:
        return "occupancy";
    case SweepIntent::interference_hunt:
        return "interference_hunt";
    case SweepIntent::calibration:
        return "calibration";
    case SweepIntent::compliance:
        return "compliance";
    }
    return "survey";
}

std::string render_sweep_segment(const SweepSegment& segment) {
    std::ostringstream out;
    out << "segment center="
        << format_frequency(segment.center_frequency_hz)
        << " span="
        << format_frequency(segment.span_hz)
        << " bin="
        << format_frequency(segment.bin_width_hz)
        << " dwell_us="
        << segment.dwell_us
        << " priority="
        << sweep_priority_name(segment.priority)
        << " intent="
        << sweep_intent_name(segment.intent)
        << " reason=\""
        << segment.reason
        << "\"";
    if (segment.channel) {
        out << " channel=" << segment.channel->label;
    }
    if (segment.allocation) {
        out << " allocation=\"" << segment.allocation->name << "\"";
    }
    return out.str();
}

std::string render_sweep_plan(const SweepPlan& plan) {
    std::ostringstream out;
    out << "sweep_plan "
        << plan.name
        << "\n"
        << "  segments: "
        << plan.segments.size()
        << "\n"
        << "  estimated_duration_us: "
        << plan.estimated_duration_us
        << "\n"
        << "  covered: "
        << format_frequency(plan.covered_hz)
        << "\n"
        << "  duty_cycle: "
        << plan.duty_cycle
        << "\n";
    for (const auto& warning : plan.warnings) {
        out << "  warning: " << warning << "\n";
    }
    for (const auto& segment : plan.segments) {
        out << "  " << render_sweep_segment(segment) << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
