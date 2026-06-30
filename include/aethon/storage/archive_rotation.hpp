#pragma once

#include "aethon/storage/archive.hpp"
#include "aethon/storage/manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

enum class RotationReason {
    none,
    max_records,
    max_payload_bytes,
    max_span_ns,
    max_file_bytes,
    idle_gap,
    explicit_boundary,
};

enum class RotationStepKind {
    keep_current,
    start_new_archive,
    close_archive,
    copy_record,
    skip_record,
};

struct RotationPolicy {
    std::uint64_t max_records_per_archive = 0;
    std::uint64_t max_payload_bytes_per_archive = 0;
    std::uint64_t max_capture_span_ns = 0;
    std::uint64_t max_estimated_file_bytes = 0;
    std::uint64_t idle_gap_ns = 0;
    bool keep_empty_archives = false;
    std::string output_prefix = "archive";
    std::string output_extension = ".ath";
};

struct RotationInputArchive {
    std::filesystem::path path;
    ArchiveManifest manifest;
};

struct RotationRecordProjection {
    std::filesystem::path source_path;
    std::uint64_t source_offset = 0;
    std::uint64_t capture_time_ns = 0;
    std::uint64_t payload_size = 0;
    protocol::DeviceId device = 0;
    std::uint32_t sequence = 0;
};

struct RotationShard {
    std::filesystem::path output_path;
    RotationReason reason = RotationReason::none;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t estimated_file_bytes = 0;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
    std::vector<RotationRecordProjection> records;
};

struct RotationStep {
    RotationStepKind kind = RotationStepKind::keep_current;
    RotationReason reason = RotationReason::none;
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    std::uint64_t capture_time_ns = 0;
    std::uint64_t source_offset = 0;
    std::string note;
};

struct RotationPlan {
    RotationPolicy policy;
    std::vector<RotationShard> shards;
    std::vector<RotationStep> steps;
    std::uint64_t input_archives = 0;
    std::uint64_t input_records = 0;
    std::uint64_t output_records = 0;
    std::uint64_t skipped_records = 0;
};

struct RotationExecutionOptions {
    bool overwrite_outputs = false;
    bool preserve_input_order = true;
};

struct RotationExecutionReport {
    RotationPlan plan;
    std::vector<std::filesystem::path> written_archives;
    std::vector<std::string> warnings;
    std::uint64_t records_written = 0;
    std::uint64_t records_skipped = 0;
};

[[nodiscard]] std::string rotation_reason_name(RotationReason reason);
[[nodiscard]] std::string rotation_step_name(RotationStepKind kind);
[[nodiscard]] std::filesystem::path rotation_output_path(const RotationPolicy& policy,
                                                         const std::filesystem::path& directory,
                                                         std::uint64_t shard_index);
[[nodiscard]] std::uint64_t estimate_archive_record_bytes(const ArchiveIndexEntry& entry);
[[nodiscard]] std::uint64_t estimate_archive_record_bytes(const ArchiveRecord& record);
[[nodiscard]] bool rotation_policy_has_limits(const RotationPolicy& policy);
[[nodiscard]] std::vector<RotationRecordProjection> project_archive_records(const std::filesystem::path& path);
[[nodiscard]] RotationPlan plan_archive_rotation(const std::vector<RotationInputArchive>& inputs,
                                                 const RotationPolicy& policy,
                                                 const std::filesystem::path& output_directory);
[[nodiscard]] RotationPlan plan_archive_rotation(const std::vector<std::filesystem::path>& inputs,
                                                 const RotationPolicy& policy,
                                                 const std::filesystem::path& output_directory);
[[nodiscard]] RotationExecutionReport execute_archive_rotation(const RotationPlan& plan,
                                                               const RotationExecutionOptions& options = {});
[[nodiscard]] std::vector<std::string> validate_rotation_plan(const RotationPlan& plan);
[[nodiscard]] std::string render_rotation_plan(const RotationPlan& plan);
[[nodiscard]] std::string render_rotation_execution_report(const RotationExecutionReport& report);

} // namespace aethon::storage
