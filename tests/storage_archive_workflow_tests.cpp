#include "test_harness.hpp"

#include "aethon/storage/archive.hpp"
#include "aethon/storage/archive_audit.hpp"
#include "aethon/storage/archive_collection.hpp"
#include "aethon/storage/archive_export_writers.hpp"
#include "aethon/storage/archive_repair_plan.hpp"
#include "aethon/storage/archive_rotation.hpp"
#include "aethon/storage/archive_sampling.hpp"
#include "aethon/storage/archive_statistics.hpp"
#include "aethon/storage/manifest_diff.hpp"
#include "aethon/storage/retention_simulator.hpp"

#include <filesystem>
#include <sstream>
#include <vector>

namespace {

aethon::protocol::Packet packet(std::uint64_t device,
                                std::uint32_t sequence,
                                std::initializer_list<std::uint8_t> payload) {
    aethon::protocol::Packet p;
    p.device = device;
    p.sequence = sequence;
    p.timestamp_ns = sequence * 100;
    p.kind = aethon::protocol::PacketKind::observation;
    p.payload.assign(payload.begin(), payload.end());
    return p;
}

void cleanup(const std::filesystem::path& path) {
    std::filesystem::remove(path);
}

void write_archive(const std::filesystem::path& path,
                   const std::vector<std::uint64_t>& times,
                   std::uint64_t device_base) {
    cleanup(path);
    aethon::storage::ArchiveWriter writer(path);
    for (std::size_t i = 0; i < times.size(); ++i) {
        writer.append(times[i], packet(device_base + (i % 2), static_cast<std::uint32_t>(i + 1), {1, 2, static_cast<std::uint8_t>(i)}));
    }
    writer.close();
}

} // namespace

AETHON_TEST(storage_archive_rotation_collection_sampling_and_export_workflow) {
    auto dir = std::filesystem::temp_directory_path();
    auto a = dir / "aethon_storage_workflow_a.ath";
    auto b = dir / "aethon_storage_workflow_b.ath";
    auto rotated0 = dir / "workflow_000000.ath";
    auto rotated1 = dir / "workflow_000001.ath";
    cleanup(a);
    cleanup(b);
    cleanup(rotated0);
    cleanup(rotated1);

    write_archive(a, {100, 200, 300}, 10);
    write_archive(b, {400, 500}, 20);

    auto audit = aethon::storage::audit_archive_integrity(a);
    AETHON_REQUIRE(aethon::storage::audit_report_ok(audit));
    AETHON_REQUIRE(audit.records_scanned == 3);

    auto collection = aethon::storage::build_archive_collection({a, b});
    AETHON_REQUIRE(collection.records.size() == 5);
    auto hit = aethon::storage::collection_record_at_or_after(collection, 350);
    AETHON_REQUIRE(hit.has_value());
    AETHON_REQUIRE(hit->capture_time_ns == 400);

    aethon::storage::ArchiveQuery query;
    query.start_time_ns = 200;
    query.min_payload_size = 3;
    auto result = aethon::storage::search_archive_collection(collection, {query, true, true});
    AETHON_REQUIRE(result.refs.size() == 4);
    AETHON_REQUIRE(result.records.size() == 4);

    aethon::storage::SamplingOptions sample_options;
    sample_options.mode = aethon::storage::SamplingMode::every_nth;
    sample_options.stride = 2;
    sample_options.limit = 3;
    auto sample = aethon::storage::sample_archive_collection(collection, sample_options);
    AETHON_REQUIRE(sample.refs.size() == 3);
    AETHON_REQUIRE(aethon::storage::render_record_sample(sample).find("record_sample") != std::string::npos);

    auto stats = aethon::storage::build_archive_statistics_report(collection, 200, 150);
    AETHON_REQUIRE(stats.time_buckets.size() == 3);
    AETHON_REQUIRE(!stats.device_kind_matrix.empty());
    AETHON_REQUIRE(aethon::storage::render_archive_statistics_report(stats).find("archive_statistics") != std::string::npos);

    aethon::storage::RotationPolicy rotation;
    rotation.max_records_per_archive = 3;
    rotation.output_prefix = "workflow";
    auto rotation_plan = aethon::storage::plan_archive_rotation({a, b}, rotation, dir);
    AETHON_REQUIRE(rotation_plan.shards.size() == 2);
    AETHON_REQUIRE(aethon::storage::validate_rotation_plan(rotation_plan).empty());
    auto rotation_report = aethon::storage::execute_archive_rotation(rotation_plan, {true, true});
    AETHON_REQUIRE(rotation_report.records_written == 5);

    auto rotated_manifest = aethon::storage::build_archive_manifest(rotated0);
    auto original_manifest = aethon::storage::build_archive_manifest(a);
    auto diff = aethon::storage::diff_archive_manifests(original_manifest, rotated_manifest);
    AETHON_REQUIRE(aethon::storage::manifest_diff_empty(diff));
    AETHON_REQUIRE(aethon::storage::render_manifest_diff(diff).find("manifest_diff") != std::string::npos);

    aethon::storage::RetentionPolicy retention;
    retention.keep_after_time_ns = 450;
    retention.archive_before_time_ns = 350;
    aethon::storage::RetentionSimulationOptions simulation_options;
    simulation_options.now_time_ns = 600;
    simulation_options.step_ns = 100;
    simulation_options.steps = 2;
    auto simulation = aethon::storage::simulate_retention({a, b}, retention, simulation_options);
    AETHON_REQUIRE(simulation.snapshots.size() == 2);
    AETHON_REQUIRE(aethon::storage::render_retention_simulation(simulation).find("retention_simulation") != std::string::npos);

    auto repair_plan = aethon::storage::plan_archive_repair(a);
    AETHON_REQUIRE(!repair_plan.actions.empty());
    AETHON_REQUIRE(aethon::storage::render_archive_repair_plan(repair_plan).find("archive_repair_plan") != std::string::npos);

    std::ostringstream out;
    aethon::storage::ArchiveRecordExportWriter writer({aethon::storage::ExportFormat::json_lines});
    writer.write_collection(out, collection);
    AETHON_REQUIRE(out.str().find("\"capture_time_ns\"") != std::string::npos);

    cleanup(a);
    cleanup(b);
    cleanup(rotated0);
    cleanup(rotated1);
}
