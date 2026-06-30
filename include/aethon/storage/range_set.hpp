#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::storage {

struct TimeRange {
    std::uint64_t start_ns = 0;
    std::uint64_t end_ns = 0;
};

class TimeRangeSet {
public:
    void add(TimeRange range);
    [[nodiscard]] bool contains(std::uint64_t time_ns) const;
    [[nodiscard]] bool overlaps(TimeRange range) const;
    [[nodiscard]] const std::vector<TimeRange>& ranges() const noexcept;
    void clear();

private:
    void normalize();

    std::vector<TimeRange> ranges_;
};

[[nodiscard]] std::string render_time_range_set(const TimeRangeSet& set);

} // namespace aethon::storage
