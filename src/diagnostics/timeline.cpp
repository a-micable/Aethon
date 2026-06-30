#include "aethon/diagnostics/timeline.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

int level_rank(TimelineLevel level) {
    switch (level) {
    case TimelineLevel::debug:
        return 0;
    case TimelineLevel::info:
        return 1;
    case TimelineLevel::warning:
        return 2;
    case TimelineLevel::error:
        return 3;
    }
    return 0;
}

} // namespace

void Timeline::add(TimelineEvent event) {
    events_.push_back(std::move(event));
}

void Timeline::add(std::uint64_t time_ns,
                   TimelineLevel level,
                   std::string source,
                   std::string message) {
    events_.push_back(TimelineEvent{
        time_ns,
        level,
        std::move(source),
        std::move(message),
    });
}

void Timeline::sort() {
    std::sort(
        events_.begin(),
        events_.end(),
        [](const TimelineEvent& left, const TimelineEvent& right) {
            if (left.time_ns != right.time_ns) {
                return left.time_ns < right.time_ns;
            }
            return level_rank(left.level) > level_rank(right.level);
        });
}

void Timeline::clear() {
    events_.clear();
}

std::vector<TimelineEvent> Timeline::between(std::uint64_t start_ns,
                                             std::uint64_t end_ns) const {
    std::vector<TimelineEvent> selected;
    for (const auto& event : events_) {
        if (event.time_ns >= start_ns && event.time_ns <= end_ns) {
            selected.push_back(event);
        }
    }
    return selected;
}

std::vector<TimelineEvent> Timeline::at_or_above(TimelineLevel level) const {
    std::vector<TimelineEvent> selected;
    auto threshold = level_rank(level);
    for (const auto& event : events_) {
        if (level_rank(event.level) >= threshold) {
            selected.push_back(event);
        }
    }
    return selected;
}

const std::vector<TimelineEvent>& Timeline::events() const noexcept {
    return events_;
}

std::string timeline_level_name(TimelineLevel level) {
    switch (level) {
    case TimelineLevel::debug:
        return "debug";
    case TimelineLevel::info:
        return "info";
    case TimelineLevel::warning:
        return "warning";
    case TimelineLevel::error:
        return "error";
    }
    return "unknown";
}

std::string render_timeline_event(const TimelineEvent& event) {
    std::ostringstream out;
    out << event.time_ns
        << " "
        << timeline_level_name(event.level)
        << " "
        << event.source
        << " "
        << event.message;
    return out.str();
}

std::string render_timeline(const Timeline& timeline) {
    std::ostringstream out;
    out << "timeline\n";
    for (const auto& event : timeline.events()) {
        out << "  "
            << render_timeline_event(event)
            << "\n";
    }
    return out.str();
}

} // namespace aethon::diagnostics
