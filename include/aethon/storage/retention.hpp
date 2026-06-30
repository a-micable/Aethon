#pragma once

#include "aethon/storage/manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

enum class RetentionAction {
    keep,
    compact,
    archive,
    delete_file,
};

struct RetentionPolicy {
    std::uint64_t keep_after_time_ns = 0;
    std::uint64_t compact_before_time_ns = 0;
    std::uint64_t archive_before_time_ns = 0;
    std::uint64_t delete_before_time_ns = 0;
    std::uint64_t minimum_records_to_keep = 0;
    std::uint64_t minimum_payload_bytes_to_keep = 0;
};

struct RetentionCandidate {
    std::filesystem::path path;
    ArchiveManifest manifest;
};

struct RetentionDecision {
    std::filesystem::path path;
    RetentionAction action = RetentionAction::keep;
    std::string reason;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t last_time_ns = 0;
};

struct RetentionPlan {
    std::vector<RetentionDecision> decisions;
    std::uint64_t kept_records = 0;
    std::uint64_t compacted_records = 0;
    std::uint64_t archived_records = 0;
    std::uint64_t deleted_records = 0;
};

[[nodiscard]] std::uint64_t manifest_payload_bytes(const ArchiveManifest& manifest);
[[nodiscard]] RetentionDecision decide_retention(const RetentionCandidate& candidate,
                                                 const RetentionPolicy& policy);
[[nodiscard]] RetentionPlan build_retention_plan(std::vector<RetentionCandidate> candidates,
                                                 const RetentionPolicy& policy);
[[nodiscard]] std::string retention_action_name(RetentionAction action);
[[nodiscard]] std::string render_retention_plan(const RetentionPlan& plan);

} // namespace aethon::storage
