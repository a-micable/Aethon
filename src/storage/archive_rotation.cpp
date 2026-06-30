#include "aethon/storage/archive_rotation.hpp"

#include "aethon/common/error.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace aethon::storage {
namespace {

constexpr std::uint64_t archive_header_bytes = 36;
constexpr std::uint64_t archive_record_overhead_bytes = 16;

struct MutableShard {
    RotationShard shard;
    bool has_records = false;
};

std::string format_shard_name(const RotationPolicy& policy, std::uint64_t shard_index) {
    std::ostringstream out;
    out << policy.output_prefix << "_" << std::setw(6) << std::setfill('0') << shard_index
        << policy.output_extension;
    return out.str();
}

RotationStep make_step(RotationStepKind kind,
                       RotationReason reason,
                       const RotationRecordProjection& record,
                       const std::filesystem::path& output,
                       std::string note) {
    RotationStep step;
    step.kind = kind;
    step.reason = reason;
    step.source_path = record.source_path;
    step.output_path = output;
    step.capture_time_ns = record.capture_time_ns;
    step.source_offset = record.source_offset;
    step.note = std::move(note);
    return step;
}

RotationStep make_archive_step(RotationStepKind kind,
                               RotationReason reason,
                               const std::filesystem::path& output,
                               std::string note) {
    RotationStep step;
    step.kind = kind;
    step.reason = reason;
    step.output_path = output;
    step.note = std::move(note);
    return step;
}

std::uint64_t estimate_file_bytes_after(const RotationShard& shard,
                                        const RotationRecordProjection& record) {
    return shard.estimated_file_bytes + archive_record_overhead_bytes + record.payload_size;
}

std::uint64_t span_after(const RotationShard& shard,
                         const RotationRecordProjection& record) {
    if (shard.record_count == 0) {
        return 0;
    }
    auto first = std::min(shard.first_time_ns, record.capture_time_ns);
    auto last = std::max(shard.last_time_ns, record.capture_time_ns);
    return last - first;
}

std::optional<RotationReason> boundary_reason(const RotationPolicy& policy,
                                              const RotationShard& shard,
                                              const RotationRecordProjection& record) {
    if (shard.record_count == 0) {
        return std::nullopt;
    }
    if (policy.max_records_per_archive != 0
        && shard.record_count >= policy.max_records_per_archive) {
        return RotationReason::max_records;
    }
    if (policy.max_payload_bytes_per_archive != 0
        && shard.payload_bytes + record.payload_size > policy.max_payload_bytes_per_archive) {
        return RotationReason::max_payload_bytes;
    }
    if (policy.max_capture_span_ns != 0
        && span_after(shard, record) > policy.max_capture_span_ns) {
        return RotationReason::max_span_ns;
    }
    if (policy.max_estimated_file_bytes != 0
        && estimate_file_bytes_after(shard, record) > policy.max_estimated_file_bytes) {
        return RotationReason::max_file_bytes;
    }
    if (policy.idle_gap_ns != 0
        && record.capture_time_ns > shard.last_time_ns
        && record.capture_time_ns - shard.last_time_ns > policy.idle_gap_ns) {
        return RotationReason::idle_gap;
    }
    return std::nullopt;
}

void append_record(RotationShard& shard, const RotationRecordProjection& record) {
    if (shard.record_count == 0) {
        shard.first_time_ns = record.capture_time_ns;
        shard.last_time_ns = record.capture_time_ns;
        shard.estimated_file_bytes = archive_header_bytes;
    } else {
        shard.first_time_ns = std::min(shard.first_time_ns, record.capture_time_ns);
        shard.last_time_ns = std::max(shard.last_time_ns, record.capture_time_ns);
    }
    ++shard.record_count;
    shard.payload_bytes += record.payload_size;
    shard.estimated_file_bytes += archive_record_overhead_bytes + record.payload_size;
    shard.records.push_back(record);
}

void close_current_shard(RotationPlan& plan,
                         MutableShard& current,
                         RotationReason next_reason) {
    if (!current.has_records && !plan.policy.keep_empty_archives) {
        return;
    }
    current.shard.reason = next_reason;
    plan.steps.push_back(make_archive_step(
        RotationStepKind::close_archive,
        next_reason,
        current.shard.output_path,
        "close rotation shard"));
    plan.output_records += current.shard.record_count;
    plan.shards.push_back(std::move(current.shard));
    current = {};
}

MutableShard start_shard(const RotationPolicy& policy,
                         const std::filesystem::path& output_directory,
                         std::uint64_t shard_index,
                         RotationReason reason,
                         RotationPlan& plan) {
    MutableShard current;
    current.shard.output_path = rotation_output_path(policy, output_directory, shard_index);
    current.shard.reason = reason;
    current.shard.estimated_file_bytes = archive_header_bytes;
    plan.steps.push_back(make_archive_step(
        RotationStepKind::start_new_archive,
        reason,
        current.shard.output_path,
        "start rotation shard"));
    return current;
}

std::vector<RotationRecordProjection> collect_inputs(const std::vector<RotationInputArchive>& inputs) {
    std::vector<RotationRecordProjection> records;
    for (const auto& input : inputs) {
        auto projected = project_archive_records(input.path);
        records.insert(records.end(), projected.begin(), projected.end());
    }
    std::stable_sort(
        records.begin(),
        records.end(),
        [](const RotationRecordProjection& left, const RotationRecordProjection& right) {
            if (left.capture_time_ns != right.capture_time_ns) {
                return left.capture_time_ns < right.capture_time_ns;
            }
            if (left.source_path != right.source_path) {
                return left.source_path.string() < right.source_path.string();
            }
            return left.source_offset < right.source_offset;
        });
    return records;
}

std::optional<ArchiveRecord> read_record_at(const std::filesystem::path& path,
                                            std::uint64_t offset) {
    ArchiveReader reader(path);
    while (auto record = reader.next()) {
        if (record->offset == offset) {
            return record;
        }
        if (record->offset > offset) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void ensure_output_available(const std::filesystem::path& path,
                             bool overwrite,
                             std::vector<std::string>& warnings) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    if (!overwrite) {
        throw Error(ErrorCode::invalid_argument, "rotation output already exists");
    }
    warnings.push_back("overwriting existing archive: " + path.string());
    std::filesystem::remove(path);
}

} // namespace

std::string rotation_reason_name(RotationReason reason) {
    switch (reason) {
    case RotationReason::none:
        return "none";
    case RotationReason::max_records:
        return "max_records";
    case RotationReason::max_payload_bytes:
        return "max_payload_bytes";
    case RotationReason::max_span_ns:
        return "max_span_ns";
    case RotationReason::max_file_bytes:
        return "max_file_bytes";
    case RotationReason::idle_gap:
        return "idle_gap";
    case RotationReason::explicit_boundary:
        return "explicit_boundary";
    }
    return "unknown";
}

std::string rotation_step_name(RotationStepKind kind) {
    switch (kind) {
    case RotationStepKind::keep_current:
        return "keep_current";
    case RotationStepKind::start_new_archive:
        return "start_new_archive";
    case RotationStepKind::close_archive:
        return "close_archive";
    case RotationStepKind::copy_record:
        return "copy_record";
    case RotationStepKind::skip_record:
        return "skip_record";
    }
    return "unknown";
}

std::filesystem::path rotation_output_path(const RotationPolicy& policy,
                                           const std::filesystem::path& directory,
                                           std::uint64_t shard_index) {
    return directory / format_shard_name(policy, shard_index);
}

std::uint64_t estimate_archive_record_bytes(const ArchiveIndexEntry& entry) {
    return archive_record_overhead_bytes + entry.payload_size;
}

std::uint64_t estimate_archive_record_bytes(const ArchiveRecord& record) {
    return archive_record_overhead_bytes + record.packet.payload.size();
}

bool rotation_policy_has_limits(const RotationPolicy& policy) {
    return policy.max_records_per_archive != 0
        || policy.max_payload_bytes_per_archive != 0
        || policy.max_capture_span_ns != 0
        || policy.max_estimated_file_bytes != 0
        || policy.idle_gap_ns != 0;
}

std::vector<RotationRecordProjection> project_archive_records(const std::filesystem::path& path) {
    auto index = build_archive_index(path);
    std::vector<RotationRecordProjection> records;
    records.reserve(index.records.size());
    for (const auto& entry : index.records) {
        records.push_back(RotationRecordProjection{
            path,
            entry.offset,
            entry.capture_time_ns,
            entry.payload_size,
            entry.device,
            entry.sequence,
        });
    }
    return records;
}

RotationPlan plan_archive_rotation(const std::vector<RotationInputArchive>& inputs,
                                   const RotationPolicy& policy,
                                   const std::filesystem::path& output_directory) {
    RotationPlan plan;
    plan.policy = policy;
    plan.input_archives = inputs.size();
    if (policy.output_prefix.empty()) {
        throw Error(ErrorCode::invalid_argument, "rotation output prefix must not be empty");
    }
    if (policy.output_extension.empty()) {
        throw Error(ErrorCode::invalid_argument, "rotation output extension must not be empty");
    }

    auto records = collect_inputs(inputs);
    plan.input_records = records.size();
    if (records.empty()) {
        if (policy.keep_empty_archives) {
            auto current = start_shard(policy, output_directory, 0, RotationReason::none, plan);
            close_current_shard(plan, current, RotationReason::none);
        }
        return plan;
    }

    std::uint64_t shard_index = 0;
    auto current = start_shard(policy, output_directory, shard_index, RotationReason::none, plan);
    for (const auto& record : records) {
        if (auto reason = boundary_reason(policy, current.shard, record)) {
            close_current_shard(plan, current, *reason);
            ++shard_index;
            current = start_shard(policy, output_directory, shard_index, *reason, plan);
        }
        current.has_records = true;
        append_record(current.shard, record);
        plan.steps.push_back(make_step(
            RotationStepKind::copy_record,
            RotationReason::none,
            record,
            current.shard.output_path,
            "copy record into rotation shard"));
    }
    close_current_shard(plan, current, RotationReason::none);
    return plan;
}

RotationPlan plan_archive_rotation(const std::vector<std::filesystem::path>& inputs,
                                   const RotationPolicy& policy,
                                   const std::filesystem::path& output_directory) {
    std::vector<RotationInputArchive> archives;
    archives.reserve(inputs.size());
    for (const auto& path : inputs) {
        archives.push_back(RotationInputArchive{path, build_archive_manifest(path)});
    }
    return plan_archive_rotation(archives, policy, output_directory);
}

RotationExecutionReport execute_archive_rotation(const RotationPlan& plan,
                                                 const RotationExecutionOptions& options) {
    auto warnings = validate_rotation_plan(plan);
    if (!warnings.empty()) {
        throw Error(ErrorCode::invalid_argument, "rotation plan failed validation");
    }

    RotationExecutionReport report;
    report.plan = plan;
    for (const auto& shard : plan.shards) {
        ensure_output_available(shard.output_path, options.overwrite_outputs, report.warnings);
        ArchiveWriter writer(shard.output_path);
        for (const auto& projection : shard.records) {
            auto record = read_record_at(projection.source_path, projection.source_offset);
            if (!record) {
                ++report.records_skipped;
                report.warnings.push_back("missing projected record at offset "
                    + std::to_string(projection.source_offset));
                continue;
            }
            writer.append(record->capture_time_ns, record->packet);
            ++report.records_written;
        }
        writer.close();
        report.written_archives.push_back(shard.output_path);
    }
    return report;
}

std::vector<std::string> validate_rotation_plan(const RotationPlan& plan) {
    std::vector<std::string> warnings;
    std::unordered_set<std::string> outputs;
    std::uint64_t counted_records = 0;
    for (const auto& shard : plan.shards) {
        if (shard.output_path.empty()) {
            warnings.push_back("rotation shard has empty output path");
        }
        auto inserted = outputs.insert(shard.output_path.string());
        if (!inserted.second) {
            warnings.push_back("rotation plan contains duplicate output path");
        }
        if (shard.record_count != shard.records.size()) {
            warnings.push_back("rotation shard record count differs from projected records");
        }
        if (shard.record_count == 0 && !plan.policy.keep_empty_archives) {
            warnings.push_back("rotation shard is empty but empty archives are disabled");
        }
        if (shard.record_count != 0 && shard.first_time_ns > shard.last_time_ns) {
            warnings.push_back("rotation shard has inverted time range");
        }
        counted_records += shard.record_count;
    }
    if (counted_records != plan.output_records) {
        warnings.push_back("rotation output record total differs from shard totals");
    }
    if (plan.output_records + plan.skipped_records != plan.input_records) {
        warnings.push_back("rotation plan does not account for every input record");
    }
    return warnings;
}

std::string render_rotation_plan(const RotationPlan& plan) {
    std::ostringstream out;
    out << "rotation_plan\n"
        << "  input_archives: " << plan.input_archives << "\n"
        << "  input_records: " << plan.input_records << "\n"
        << "  output_records: " << plan.output_records << "\n"
        << "  skipped_records: " << plan.skipped_records << "\n"
        << "  shards: " << plan.shards.size() << "\n";
    for (const auto& shard : plan.shards) {
        out << "  shard: " << shard.output_path.string()
            << " reason=" << rotation_reason_name(shard.reason)
            << " records=" << shard.record_count
            << " payload_bytes=" << shard.payload_bytes
            << " estimated_file_bytes=" << shard.estimated_file_bytes
            << " first_time_ns=" << shard.first_time_ns
            << " last_time_ns=" << shard.last_time_ns
            << "\n";
    }
    for (const auto& step : plan.steps) {
        out << "  step: " << rotation_step_name(step.kind)
            << " reason=" << rotation_reason_name(step.reason)
            << " source=" << step.source_path.string()
            << " output=" << step.output_path.string()
            << " offset=" << step.source_offset
            << " capture_time_ns=" << step.capture_time_ns
            << " note=\"" << step.note << "\"\n";
    }
    return out.str();
}

std::string render_rotation_execution_report(const RotationExecutionReport& report) {
    std::ostringstream out;
    out << "rotation_execution\n"
        << "  written_archives: " << report.written_archives.size() << "\n"
        << "  records_written: " << report.records_written << "\n"
        << "  records_skipped: " << report.records_skipped << "\n";
    for (const auto& archive : report.written_archives) {
        out << "  archive: " << archive.string() << "\n";
    }
    for (const auto& warning : report.warnings) {
        out << "  warning: " << warning << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
