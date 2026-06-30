#pragma once

#include "aethon/storage/archive_audit.hpp"
#include "aethon/storage/repair.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

enum class RepairActionKind {
    no_op,
    rewrite_from_readable_records,
    rebuild_from_salvage,
    quarantine_file,
    verify_after_write,
};

struct RepairPlannerOptions {
    bool allow_salvage_scan = true;
    bool quarantine_unreadable = true;
    bool rewrite_when_header_mismatch = true;
    bool rewrite_when_unordered = false;
    std::filesystem::path output_directory;
    std::string repaired_suffix = ".repaired.ath";
    std::string quarantine_suffix = ".quarantine";
};

struct RepairAction {
    RepairActionKind kind = RepairActionKind::no_op;
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    std::string reason;
    std::uint64_t expected_records = 0;
};

struct ArchiveRepairPlan {
    std::filesystem::path source_path;
    ArchiveAuditReport audit;
    RepairReport salvage;
    std::vector<RepairAction> actions;
    std::uint64_t readable_records = 0;
    std::uint64_t salvaged_records = 0;
    bool uses_salvage = false;
};

struct RepairExecutionReport {
    ArchiveRepairPlan plan;
    std::vector<std::filesystem::path> outputs;
    std::vector<std::string> warnings;
    std::uint64_t actions_executed = 0;
    std::uint64_t records_written = 0;
};

[[nodiscard]] std::string repair_action_name(RepairActionKind kind);
[[nodiscard]] ArchiveRepairPlan plan_archive_repair(const std::filesystem::path& path,
                                                    const RepairPlannerOptions& options = {});
[[nodiscard]] std::vector<ArchiveRepairPlan> plan_archive_repairs(const std::vector<std::filesystem::path>& paths,
                                                                  const RepairPlannerOptions& options = {});
[[nodiscard]] RepairExecutionReport execute_archive_repair_plan(const ArchiveRepairPlan& plan,
                                                                bool overwrite_outputs = false);
[[nodiscard]] std::vector<std::string> validate_archive_repair_plan(const ArchiveRepairPlan& plan);
[[nodiscard]] std::string render_archive_repair_plan(const ArchiveRepairPlan& plan);
[[nodiscard]] std::string render_repair_execution_report(const RepairExecutionReport& report);

} // namespace aethon::storage
