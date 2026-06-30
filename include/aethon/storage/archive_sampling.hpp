#pragma once

#include "aethon/storage/archive_collection.hpp"
#include "aethon/storage/query.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aethon::storage {

enum class SamplingMode {
    first_n,
    every_nth,
    time_bucket_first,
    deterministic_hash,
    payload_extremes,
};

struct SamplingOptions {
    SamplingMode mode = SamplingMode::first_n;
    std::size_t limit = 0;
    std::uint64_t stride = 1;
    std::uint64_t bucket_width_ns = 0;
    std::uint64_t hash_modulus = 1;
    std::uint64_t hash_remainder = 0;
    bool include_records = true;
};

struct SampledRecordRef {
    ArchiveCollectionRecordRef ref;
    std::string reason;
    std::uint64_t sample_rank = 0;
};

struct RecordSample {
    SamplingOptions options;
    std::vector<SampledRecordRef> refs;
    std::vector<ArchiveRecord> records;
    std::uint64_t candidates_seen = 0;
    std::uint64_t selected_count = 0;
    std::uint64_t rejected_count = 0;
};

struct PayloadDistributionBin {
    std::uint64_t min_payload_size = 0;
    std::uint64_t max_payload_size = 0;
    std::uint64_t record_count = 0;
};

[[nodiscard]] std::string sampling_mode_name(SamplingMode mode);
[[nodiscard]] std::uint64_t deterministic_record_hash(const ArchiveCollectionRecordRef& ref);
[[nodiscard]] RecordSample sample_archive_records(const std::filesystem::path& path,
                                                  const SamplingOptions& options,
                                                  const ArchiveQuery& query = {});
[[nodiscard]] RecordSample sample_archive_collection(const ArchiveCollection& collection,
                                                     const SamplingOptions& options,
                                                     const ArchiveQuery& query = {});
[[nodiscard]] std::vector<PayloadDistributionBin> build_payload_distribution(const ArchiveCollection& collection,
                                                                             std::uint64_t bin_width);
[[nodiscard]] std::optional<ArchiveCollectionRecordRef> find_smallest_payload_record(const ArchiveCollection& collection,
                                                                                    const ArchiveQuery& query = {});
[[nodiscard]] std::optional<ArchiveCollectionRecordRef> find_largest_payload_record(const ArchiveCollection& collection,
                                                                                   const ArchiveQuery& query = {});
[[nodiscard]] std::string render_record_sample(const RecordSample& sample);
[[nodiscard]] std::string render_payload_distribution(const std::vector<PayloadDistributionBin>& bins);

} // namespace aethon::storage
