#pragma once

#include "aethon/storage/query.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

struct CompactionOptions {
    ArchiveQuery query;
    bool drop_empty_payloads = false;
    bool keep_latest_per_device = false;
};

struct CompactionStats {
    std::uint64_t records_read = 0;
    std::uint64_t records_written = 0;
    std::uint64_t records_dropped_by_query = 0;
    std::uint64_t records_dropped_empty = 0;
    std::uint64_t records_dropped_superseded = 0;
};

struct CompactionPlan {
    std::filesystem::path source;
    std::filesystem::path destination;
    CompactionOptions options;
    CompactionStats stats;
    std::vector<std::string> notes;
};

[[nodiscard]] CompactionPlan plan_compaction(const std::filesystem::path& source,
                                             const std::filesystem::path& destination,
                                             const CompactionOptions& options);

CompactionStats compact_archive(const std::filesystem::path& source,
                                const std::filesystem::path& destination,
                                const CompactionOptions& options);

[[nodiscard]] std::string render_compaction_plan(const CompactionPlan& plan);
[[nodiscard]] std::string render_compaction_stats(const CompactionStats& stats);

} // namespace aethon::storage
