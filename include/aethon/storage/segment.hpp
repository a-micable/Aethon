#pragma once

#include "aethon/storage/catalog.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

struct SegmentNaming {
    std::string prefix = "aethon";
    std::string extension = ".ath";
    std::uint64_t segment_width_ns = 3'600'000'000'000ULL;
};

struct SegmentRange {
    std::uint64_t start_time_ns = 0;
    std::uint64_t end_time_ns = 0;
};

struct SegmentPlanEntry {
    SegmentRange range;
    std::filesystem::path path;
    std::vector<std::filesystem::path> source_archives;
};

struct SegmentPlan {
    std::vector<SegmentPlanEntry> entries;
};

[[nodiscard]] SegmentRange segment_for_time(std::uint64_t time_ns, const SegmentNaming& naming);
[[nodiscard]] std::filesystem::path segment_path(const std::filesystem::path& directory,
                                                 const SegmentRange& range,
                                                 const SegmentNaming& naming);
[[nodiscard]] SegmentPlan plan_segments(const ArchiveCatalog& catalog,
                                        const std::filesystem::path& directory,
                                        const SegmentNaming& naming = {});
[[nodiscard]] std::string render_segment_plan(const SegmentPlan& plan);

} // namespace aethon::storage
