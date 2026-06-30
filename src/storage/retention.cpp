#include "aethon/storage/retention.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

std::uint64_t record_count(const ArchiveManifest& manifest) {
    return manifest.summary.record_count;
}

std::uint64_t last_time(const ArchiveManifest& manifest) {
    return manifest.summary.last_time_ns;
}

RetentionDecision make_decision(const RetentionCandidate& candidate,
                                RetentionAction action,
                                std::string reason) {
    RetentionDecision decision;
    decision.path = candidate.path;
    decision.action = action;
    decision.reason = std::move(reason);
    decision.record_count = record_count(candidate.manifest);
    decision.payload_bytes = manifest_payload_bytes(candidate.manifest);
    decision.last_time_ns = last_time(candidate.manifest);
    return decision;
}

bool protected_by_record_floor(const ArchiveManifest& manifest, const RetentionPolicy& policy) {
    return policy.minimum_records_to_keep != 0
        && manifest.summary.record_count < policy.minimum_records_to_keep;
}

bool protected_by_payload_floor(const ArchiveManifest& manifest, const RetentionPolicy& policy) {
    return policy.minimum_payload_bytes_to_keep != 0
        && manifest_payload_bytes(manifest) < policy.minimum_payload_bytes_to_keep;
}

void accumulate(RetentionPlan& plan, const RetentionDecision& decision) {
    switch (decision.action) {
    case RetentionAction::keep:
        plan.kept_records += decision.record_count;
        break;
    case RetentionAction::compact:
        plan.compacted_records += decision.record_count;
        break;
    case RetentionAction::archive:
        plan.archived_records += decision.record_count;
        break;
    case RetentionAction::delete_file:
        plan.deleted_records += decision.record_count;
        break;
    }
}

} // namespace

std::uint64_t manifest_payload_bytes(const ArchiveManifest& manifest) {
    std::uint64_t total = 0;
    for (const auto& device : manifest.devices) {
        total += device.total_payload_bytes;
    }
    return total;
}

RetentionDecision decide_retention(const RetentionCandidate& candidate,
                                   const RetentionPolicy& policy) {
    const auto& manifest = candidate.manifest;
    if (manifest.summary.record_count == 0) {
        return make_decision(candidate, RetentionAction::delete_file, "empty archive");
    }

    if (protected_by_record_floor(manifest, policy)) {
        return make_decision(candidate, RetentionAction::keep, "below minimum record floor");
    }

    if (protected_by_payload_floor(manifest, policy)) {
        return make_decision(candidate, RetentionAction::keep, "below minimum payload floor");
    }

    auto last = manifest.summary.last_time_ns;
    if (policy.keep_after_time_ns != 0 && last >= policy.keep_after_time_ns) {
        return make_decision(candidate, RetentionAction::keep, "inside active retention window");
    }

    if (policy.delete_before_time_ns != 0 && last < policy.delete_before_time_ns) {
        return make_decision(candidate, RetentionAction::delete_file, "older than delete threshold");
    }

    if (policy.archive_before_time_ns != 0 && last < policy.archive_before_time_ns) {
        return make_decision(candidate, RetentionAction::archive, "older than archive threshold");
    }

    if (policy.compact_before_time_ns != 0 && last < policy.compact_before_time_ns) {
        return make_decision(candidate, RetentionAction::compact, "older than compact threshold");
    }

    return make_decision(candidate, RetentionAction::keep, "no retention rule matched");
}

RetentionPlan build_retention_plan(std::vector<RetentionCandidate> candidates,
                                   const RetentionPolicy& policy) {
    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const RetentionCandidate& left, const RetentionCandidate& right) {
            return left.manifest.summary.last_time_ns < right.manifest.summary.last_time_ns;
        });

    RetentionPlan plan;
    plan.decisions.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        auto decision = decide_retention(candidate, policy);
        accumulate(plan, decision);
        plan.decisions.push_back(std::move(decision));
    }
    return plan;
}

std::string retention_action_name(RetentionAction action) {
    switch (action) {
    case RetentionAction::keep:
        return "keep";
    case RetentionAction::compact:
        return "compact";
    case RetentionAction::archive:
        return "archive";
    case RetentionAction::delete_file:
        return "delete";
    }
    return "unknown";
}

std::string render_retention_plan(const RetentionPlan& plan) {
    std::ostringstream out;
    out << "retention_plan\n"
        << "  decisions: "
        << plan.decisions.size()
        << "\n"
        << "  kept_records: "
        << plan.kept_records
        << "\n"
        << "  compacted_records: "
        << plan.compacted_records
        << "\n"
        << "  archived_records: "
        << plan.archived_records
        << "\n"
        << "  deleted_records: "
        << plan.deleted_records
        << "\n";

    for (const auto& decision : plan.decisions) {
        out << "  file: "
            << decision.path.string()
            << " action="
            << retention_action_name(decision.action)
            << " records="
            << decision.record_count
            << " payload_bytes="
            << decision.payload_bytes
            << " last_time_ns="
            << decision.last_time_ns
            << " reason=\""
            << decision.reason
            << "\"\n";
    }
    return out.str();
}

} // namespace aethon::storage
