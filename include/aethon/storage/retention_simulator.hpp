#pragma once

#include "aethon/storage/retention.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

struct RetentionSimulationOptions {
    std::uint64_t now_time_ns = 0;
    std::uint64_t step_ns = 0;
    std::uint32_t steps = 1;
    bool age_thresholds_with_time = true;
};

struct RetentionSimulationSnapshot {
    std::uint64_t simulated_time_ns = 0;
    RetentionPolicy policy;
    RetentionPlan plan;
    std::uint64_t kept_files = 0;
    std::uint64_t compacted_files = 0;
    std::uint64_t archived_files = 0;
    std::uint64_t deleted_files = 0;
};

struct RetentionSimulation {
    RetentionSimulationOptions options;
    std::vector<RetentionSimulationSnapshot> snapshots;
    std::uint64_t total_kept_records = 0;
    std::uint64_t total_compacted_records = 0;
    std::uint64_t total_archived_records = 0;
    std::uint64_t total_deleted_records = 0;
};

[[nodiscard]] RetentionSimulation simulate_retention(const std::vector<RetentionCandidate>& candidates,
                                                     const RetentionPolicy& policy,
                                                     const RetentionSimulationOptions& options);
[[nodiscard]] RetentionSimulation simulate_retention(const std::vector<std::filesystem::path>& paths,
                                                     const RetentionPolicy& policy,
                                                     const RetentionSimulationOptions& options);
[[nodiscard]] RetentionPolicy advance_retention_policy(const RetentionPolicy& policy,
                                                       std::uint64_t delta_ns,
                                                       bool age_thresholds);
[[nodiscard]] std::vector<RetentionDecision> decisions_for_action(const RetentionPlan& plan,
                                                                  RetentionAction action);
[[nodiscard]] std::string render_retention_simulation(const RetentionSimulation& simulation);
[[nodiscard]] std::string render_retention_snapshot(const RetentionSimulationSnapshot& snapshot);

} // namespace aethon::storage
