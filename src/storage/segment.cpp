#include "aethon/storage/segment.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace aethon::storage {
namespace {

SegmentPlanEntry* find_entry(std::vector<SegmentPlanEntry>& entries, const SegmentRange& range) {
    auto it = std::find_if(
        entries.begin(),
        entries.end(),
        [&](const SegmentPlanEntry& entry) {
            return entry.range.start_time_ns == range.start_time_ns
                && entry.range.end_time_ns == range.end_time_ns;
        });
    if (it == entries.end()) {
        return nullptr;
    }
    return &*it;
}

std::string padded_time(std::uint64_t time_ns) {
    std::ostringstream out;
    out << std::setw(20)
        << std::setfill('0')
        << time_ns;
    return out.str();
}

} // namespace

SegmentRange segment_for_time(std::uint64_t time_ns, const SegmentNaming& naming) {
    auto width = naming.segment_width_ns == 0 ? 1 : naming.segment_width_ns;
    auto start = (time_ns / width) * width;
    return SegmentRange{
        start,
        start + width - 1,
    };
}

std::filesystem::path segment_path(const std::filesystem::path& directory,
                                   const SegmentRange& range,
                                   const SegmentNaming& naming) {
    std::ostringstream name;
    name << naming.prefix
         << "-"
         << padded_time(range.start_time_ns)
         << "-"
         << padded_time(range.end_time_ns)
         << naming.extension;
    return directory / name.str();
}

SegmentPlan plan_segments(const ArchiveCatalog& catalog,
                          const std::filesystem::path& directory,
                          const SegmentNaming& naming) {
    SegmentPlan plan;
    for (const auto& entry : catalog.entries()) {
        if (entry.manifest.summary.record_count == 0) {
            continue;
        }
        auto first = segment_for_time(entry.manifest.summary.first_time_ns, naming);
        auto last = segment_for_time(entry.manifest.summary.last_time_ns, naming);
        for (auto start = first.start_time_ns;
             start <= last.start_time_ns;
             start += naming.segment_width_ns == 0 ? 1 : naming.segment_width_ns) {
            auto range = segment_for_time(start, naming);
            auto* existing = find_entry(plan.entries, range);
            if (existing == nullptr) {
                SegmentPlanEntry planned;
                planned.range = range;
                planned.path = segment_path(directory, range, naming);
                planned.source_archives.push_back(entry.path);
                plan.entries.push_back(std::move(planned));
            } else {
                existing->source_archives.push_back(entry.path);
            }
            if (start == last.start_time_ns) {
                break;
            }
        }
    }
    std::sort(
        plan.entries.begin(),
        plan.entries.end(),
        [](const SegmentPlanEntry& left, const SegmentPlanEntry& right) {
            return left.range.start_time_ns < right.range.start_time_ns;
        });
    return plan;
}

std::string render_segment_plan(const SegmentPlan& plan) {
    std::ostringstream out;
    out << "segment_plan\n"
        << "  entries: "
        << plan.entries.size()
        << "\n";
    for (const auto& entry : plan.entries) {
        out << "  segment: "
            << entry.path.string()
            << " start="
            << entry.range.start_time_ns
            << " end="
            << entry.range.end_time_ns
            << " sources="
            << entry.source_archives.size()
            << "\n";
        for (const auto& source : entry.source_archives) {
            out << "    source: "
                << source.string()
                << "\n";
        }
    }
    return out.str();
}

} // namespace aethon::storage
