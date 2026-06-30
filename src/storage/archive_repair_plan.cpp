#include "aethon/storage/archive_repair_plan.hpp"

#include "aethon/common/error.hpp"
#include "aethon/storage/archive.hpp"

#include <algorithm>
#include <sstream>

namespace aethon::storage {
namespace {

std::filesystem::path output_for(const std::filesystem::path& source,
                                 const RepairPlannerOptions& options,
                                 const std::string& suffix) {
    auto directory = options.output_directory.empty()
        ? source.parent_path()
        : options.output_directory;
    auto name = source.filename().string() + suffix;
    return directory / name;
}

bool has_check(const ArchiveAuditReport& audit, AuditCheck check) {
    return std::any_of(
        audit.findings.begin(),
        audit.findings.end(),
        [check](const AuditFinding& finding) {
            return finding.check == check;
        });
}

bool has_error(const ArchiveAuditReport& audit) {
    return std::any_of(
        audit.findings.begin(),
        audit.findings.end(),
        [](const AuditFinding& finding) {
            return finding.severity == AuditSeverity::error;
        });
}

RepairAction make_action(RepairActionKind kind,
                         const std::filesystem::path& source,
                         const std::filesystem::path& output,
                         std::string reason,
                         std::uint64_t expected_records) {
    RepairAction action;
    action.kind = kind;
    action.source_path = source;
    action.output_path = output;
    action.reason = std::move(reason);
    action.expected_records = expected_records;
    return action;
}

std::uint64_t copy_readable_records(const std::filesystem::path& source,
                                    const std::filesystem::path& output) {
    ArchiveReader reader(source);
    ArchiveWriter writer(output);
    std::uint64_t written = 0;
    while (auto record = reader.next()) {
        writer.append(record->capture_time_ns, record->packet);
        ++written;
    }
    writer.close();
    return written;
}

void ensure_can_write(const std::filesystem::path& path, bool overwrite) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    if (!overwrite) {
        throw Error(ErrorCode::invalid_argument, "repair output already exists");
    }
    std::filesystem::remove(path);
}

} // namespace

std::string repair_action_name(RepairActionKind kind) {
    switch (kind) {
    case RepairActionKind::no_op:
        return "no_op";
    case RepairActionKind::rewrite_from_readable_records:
        return "rewrite_from_readable_records";
    case RepairActionKind::rebuild_from_salvage:
        return "rebuild_from_salvage";
    case RepairActionKind::quarantine_file:
        return "quarantine_file";
    case RepairActionKind::verify_after_write:
        return "verify_after_write";
    }
    return "unknown";
}

ArchiveRepairPlan plan_archive_repair(const std::filesystem::path& path,
                                      const RepairPlannerOptions& options) {
    ArchiveRepairPlan plan;
    plan.source_path = path;
    plan.audit = audit_archive_integrity(path);
    plan.readable_records = plan.audit.records_scanned;

    if (options.allow_salvage_scan && (!plan.audit.readable || has_error(plan.audit))) {
        ArchiveRepairScanner scanner;
        plan.salvage = scanner.scan_file(path);
        plan.salvaged_records = plan.salvage.packets.size();
    }

    if (audit_report_ok(plan.audit)) {
        plan.actions.push_back(make_action(
            RepairActionKind::no_op,
            path,
            {},
            "archive passed integrity audit",
            plan.readable_records));
        return plan;
    }

    auto repaired_output = output_for(path, options, options.repaired_suffix);
    if (plan.salvaged_records > plan.readable_records) {
        plan.uses_salvage = true;
        plan.actions.push_back(make_action(
            RepairActionKind::rebuild_from_salvage,
            path,
            repaired_output,
            "salvage scan found more packets than readable archive scan",
            plan.salvaged_records));
        plan.actions.push_back(make_action(
            RepairActionKind::verify_after_write,
            repaired_output,
            repaired_output,
            "verify rebuilt archive",
            plan.salvaged_records));
        return plan;
    }

    bool should_rewrite = false;
    std::string reason;
    if (options.rewrite_when_header_mismatch
        && has_check(plan.audit, AuditCheck::header_matches_records)) {
        should_rewrite = true;
        reason = "rewrite archive to refresh inconsistent header";
    }
    if (options.rewrite_when_unordered
        && has_check(plan.audit, AuditCheck::ordered_timestamps)) {
        should_rewrite = true;
        reason = "rewrite archive after timestamp ordering finding";
    }
    if (should_rewrite && plan.audit.readable) {
        plan.actions.push_back(make_action(
            RepairActionKind::rewrite_from_readable_records,
            path,
            repaired_output,
            reason,
            plan.readable_records));
        plan.actions.push_back(make_action(
            RepairActionKind::verify_after_write,
            repaired_output,
            repaired_output,
            "verify rewritten archive",
            plan.readable_records));
        return plan;
    }

    if (options.quarantine_unreadable) {
        plan.actions.push_back(make_action(
            RepairActionKind::quarantine_file,
            path,
            output_for(path, options, options.quarantine_suffix),
            "archive could not be safely repaired",
            0));
        return plan;
    }

    plan.actions.push_back(make_action(
        RepairActionKind::no_op,
        path,
        {},
        "repair planner found no enabled remediation",
        plan.readable_records));
    return plan;
}

std::vector<ArchiveRepairPlan> plan_archive_repairs(const std::vector<std::filesystem::path>& paths,
                                                   const RepairPlannerOptions& options) {
    std::vector<ArchiveRepairPlan> plans;
    plans.reserve(paths.size());
    for (const auto& path : paths) {
        plans.push_back(plan_archive_repair(path, options));
    }
    return plans;
}

RepairExecutionReport execute_archive_repair_plan(const ArchiveRepairPlan& plan,
                                                  bool overwrite_outputs) {
    auto validation = validate_archive_repair_plan(plan);
    if (!validation.empty()) {
        throw Error(ErrorCode::invalid_argument, "archive repair plan failed validation");
    }
    RepairExecutionReport report;
    report.plan = plan;
    for (const auto& action : plan.actions) {
        switch (action.kind) {
        case RepairActionKind::no_op:
            break;
        case RepairActionKind::rewrite_from_readable_records:
            ensure_can_write(action.output_path, overwrite_outputs);
            report.records_written += copy_readable_records(action.source_path, action.output_path);
            report.outputs.push_back(action.output_path);
            break;
        case RepairActionKind::rebuild_from_salvage:
            ensure_can_write(action.output_path, overwrite_outputs);
            write_repaired_archive(action.output_path, plan.salvage);
            report.records_written += plan.salvage.packets.size();
            report.outputs.push_back(action.output_path);
            break;
        case RepairActionKind::quarantine_file:
            ensure_can_write(action.output_path, overwrite_outputs);
            std::filesystem::rename(action.source_path, action.output_path);
            report.outputs.push_back(action.output_path);
            break;
        case RepairActionKind::verify_after_write: {
            auto audit = audit_archive_integrity(action.output_path);
            if (!audit_report_ok(audit)) {
                report.warnings.push_back("verification audit reported findings for "
                    + action.output_path.string());
            }
            break;
        }
        }
        ++report.actions_executed;
    }
    return report;
}

std::vector<std::string> validate_archive_repair_plan(const ArchiveRepairPlan& plan) {
    std::vector<std::string> warnings;
    if (plan.source_path.empty()) {
        warnings.push_back("repair plan has empty source path");
    }
    if (plan.actions.empty()) {
        warnings.push_back("repair plan has no actions");
    }
    for (const auto& action : plan.actions) {
        if (action.kind != RepairActionKind::no_op && action.output_path.empty()) {
            warnings.push_back("repair action requires an output path");
        }
        if (action.kind == RepairActionKind::rebuild_from_salvage
            && plan.salvage.packets.empty()) {
            warnings.push_back("salvage rebuild action has no salvaged packets");
        }
        if (action.kind == RepairActionKind::rewrite_from_readable_records
            && !plan.audit.readable) {
            warnings.push_back("readable rewrite action cannot run on unreadable archive");
        }
    }
    return warnings;
}

std::string render_archive_repair_plan(const ArchiveRepairPlan& plan) {
    std::ostringstream out;
    out << "archive_repair_plan\n"
        << "  source: " << plan.source_path.string() << "\n"
        << "  readable_records: " << plan.readable_records << "\n"
        << "  salvaged_records: " << plan.salvaged_records << "\n"
        << "  uses_salvage: " << (plan.uses_salvage ? "true" : "false") << "\n";
    for (const auto& action : plan.actions) {
        out << "  action: " << repair_action_name(action.kind)
            << " source=" << action.source_path.string()
            << " output=" << action.output_path.string()
            << " expected_records=" << action.expected_records
            << " reason=\"" << action.reason << "\"\n";
    }
    return out.str();
}

std::string render_repair_execution_report(const RepairExecutionReport& report) {
    std::ostringstream out;
    out << "repair_execution\n"
        << "  actions_executed: " << report.actions_executed << "\n"
        << "  records_written: " << report.records_written << "\n";
    for (const auto& output : report.outputs) {
        out << "  output: " << output.string() << "\n";
    }
    for (const auto& warning : report.warnings) {
        out << "  warning: " << warning << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
