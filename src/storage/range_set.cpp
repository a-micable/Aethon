#include "aethon/storage/range_set.hpp"

#include <algorithm>
#include <sstream>

namespace aethon::storage {

void TimeRangeSet::add(TimeRange range) {
    if (range.end_ns < range.start_ns) {
        std::swap(range.start_ns, range.end_ns);
    }
    ranges_.push_back(range);
    normalize();
}

bool TimeRangeSet::contains(std::uint64_t time_ns) const {
    return std::any_of(
        ranges_.begin(),
        ranges_.end(),
        [time_ns](const TimeRange& range) {
            return time_ns >= range.start_ns && time_ns <= range.end_ns;
        });
}

bool TimeRangeSet::overlaps(TimeRange range) const {
    if (range.end_ns < range.start_ns) {
        std::swap(range.start_ns, range.end_ns);
    }
    return std::any_of(
        ranges_.begin(),
        ranges_.end(),
        [range](const TimeRange& existing) {
            return range.start_ns <= existing.end_ns && range.end_ns >= existing.start_ns;
        });
}

const std::vector<TimeRange>& TimeRangeSet::ranges() const noexcept {
    return ranges_;
}

void TimeRangeSet::clear() {
    ranges_.clear();
}

void TimeRangeSet::normalize() {
    std::sort(
        ranges_.begin(),
        ranges_.end(),
        [](const TimeRange& left, const TimeRange& right) {
            return left.start_ns < right.start_ns;
        });
    std::vector<TimeRange> merged;
    for (const auto& range : ranges_) {
        if (merged.empty() || range.start_ns > merged.back().end_ns + 1) {
            merged.push_back(range);
        } else {
            merged.back().end_ns = std::max(merged.back().end_ns, range.end_ns);
        }
    }
    ranges_ = std::move(merged);
}

std::string render_time_range_set(const TimeRangeSet& set) {
    std::ostringstream out;
    out << "time_ranges\n";
    for (const auto& range : set.ranges()) {
        out << "  range: "
            << range.start_ns
            << "-"
            << range.end_ns
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
