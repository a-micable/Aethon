#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::diagnostics {

enum class TimelineLevel {
    debug,
    info,
    warning,
    error,
};

struct TimelineEvent {
    std::uint64_t time_ns = 0;
    TimelineLevel level = TimelineLevel::info;
    std::string source;
    std::string message;
};

class Timeline {
public:
    void add(TimelineEvent event);
    void add(std::uint64_t time_ns, TimelineLevel level, std::string source, std::string message);
    void sort();
    void clear();

    [[nodiscard]] std::vector<TimelineEvent> between(std::uint64_t start_ns,
                                                     std::uint64_t end_ns) const;
    [[nodiscard]] std::vector<TimelineEvent> at_or_above(TimelineLevel level) const;
    [[nodiscard]] const std::vector<TimelineEvent>& events() const noexcept;

private:
    std::vector<TimelineEvent> events_;
};

[[nodiscard]] std::string timeline_level_name(TimelineLevel level);
[[nodiscard]] std::string render_timeline_event(const TimelineEvent& event);
[[nodiscard]] std::string render_timeline(const Timeline& timeline);

} // namespace aethon::diagnostics
