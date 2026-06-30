#include "aethon/storage/retention_simulator.hpp"

#include <sstream>

namespace aethon::storage {
namespace {

RetentionSimulationSnapshot make_snapshot(std::uint64_t simulated_time_ns,
                                           const RetentionPolicy& policy,
                                           const RetentionPlan& plan) {
    RetentionSimulationSnapshot snapshot;
    snapshot.simulated_time_ns = simulated_time_ns;
    snapshot.policy = policy;
    snapshot.plan = plan;
    for (const auto& decision : plan.decisions) {
        switch (decision.action) {
        case RetentionAction::keep:
            ++snapshot.kept_files;
            break;
        case RetentionAction::compact:
            ++snapshot.compacted_files;
            break;
        case RetentionAction::archive:
            ++snapshot.archived_files;
            break;
        case RetentionAction::delete_file:
            ++snapshot.deleted_files;
            break;
        }
    }
    return snapshot;
}

void accumulate(RetentionSimulation& simulation, const RetentionSimulationSnapshot& snapshot) {
    simulation.total_kept_records += snapshot.plan.kept_records;
    simulation.total_compacted_records += snapshot.plan.compacted_records;
    simulation.total_archived_records += snapshot.plan.archived_records;
    simulation.total_deleted_records += snapshot.plan.deleted_records;
}

std::vector<RetentionCandidate> candidates_from_paths(const std::vector<std::filesystem::path>& paths) {
    std::vector<RetentionCandidate> candidates;
    candidates.reserve(paths.size());
    for (const auto& path : paths) {
        candidates.push_back(RetentionCandidate{path, build_archive_manifest(path)});
    }
    return candidates;
}

} // namespace

RetentionSimulation simulate_retention(const std::vector<RetentionCandidate>& candidates,
                                       const RetentionPolicy& policy,
                                       const RetentionSimulationOptions& options) {
    RetentionSimulation simulation;
    simulation.options = options;
    auto steps = options.steps == 0 ? 1 : options.steps;
    for (std::uint32_t step = 0; step < steps; ++step) {
        auto delta = static_cast<std::uint64_t>(step) * options.step_ns;
        auto step_policy = advance_retention_policy(policy, delta, options.age_thresholds_with_time);
        auto plan = build_retention_plan(candidates, step_policy);
        auto snapshot = make_snapshot(options.now_time_ns + delta, step_policy, plan);
        accumulate(simulation, snapshot);
        simulation.snapshots.push_back(std::move(snapshot));
    }
    return simulation;
}

RetentionSimulation simulate_retention(const std::vector<std::filesystem::path>& paths,
                                       const RetentionPolicy& policy,
                                       const RetentionSimulationOptions& options) {
    return simulate_retention(candidates_from_paths(paths), policy, options);
}

RetentionPolicy advance_retention_policy(const RetentionPolicy& policy,
                                         std::uint64_t delta_ns,
                                         bool age_thresholds) {
    if (!age_thresholds || delta_ns == 0) {
        return policy;
    }
    RetentionPolicy advanced = policy;
    if (advanced.keep_after_time_ns != 0) {
        advanced.keep_after_time_ns += delta_ns;
    }
    if (advanced.compact_before_time_ns != 0) {
        advanced.compact_before_time_ns += delta_ns;
    }
    if (advanced.archive_before_time_ns != 0) {
        advanced.archive_before_time_ns += delta_ns;
    }
    if (advanced.delete_before_time_ns != 0) {
        advanced.delete_before_time_ns += delta_ns;
    }
    return advanced;
}

std::vector<RetentionDecision> decisions_for_action(const RetentionPlan& plan,
                                                    RetentionAction action) {
    std::vector<RetentionDecision> decisions;
    for (const auto& decision : plan.decisions) {
        if (decision.action == action) {
            decisions.push_back(decision);
        }
    }
    return decisions;
}

std::string render_retention_snapshot(const RetentionSimulationSnapshot& snapshot) {
    std::ostringstream out;
    out << "retention_snapshot\n"
        << "  simulated_time_ns: " << snapshot.simulated_time_ns << "\n"
        << "  kept_files: " << snapshot.kept_files << "\n"
        << "  compacted_files: " << snapshot.compacted_files << "\n"
        << "  archived_files: " << snapshot.archived_files << "\n"
        << "  deleted_files: " << snapshot.deleted_files << "\n"
        << "  kept_records: " << snapshot.plan.kept_records << "\n"
        << "  compacted_records: " << snapshot.plan.compacted_records << "\n"
        << "  archived_records: " << snapshot.plan.archived_records << "\n"
        << "  deleted_records: " << snapshot.plan.deleted_records << "\n";
    return out.str();
}

std::string render_retention_simulation(const RetentionSimulation& simulation) {
    std::ostringstream out;
    out << "retention_simulation\n"
        << "  snapshots: " << simulation.snapshots.size() << "\n"
        << "  total_kept_records: " << simulation.total_kept_records << "\n"
        << "  total_compacted_records: " << simulation.total_compacted_records << "\n"
        << "  total_archived_records: " << simulation.total_archived_records << "\n"
        << "  total_deleted_records: " << simulation.total_deleted_records << "\n";
    for (const auto& snapshot : simulation.snapshots) {
        out << render_retention_snapshot(snapshot);
    }
    return out.str();
}

} // namespace aethon::storage
