#include "aethon/storage/archive_sampling.hpp"

#include <algorithm>
#include <map>
#include <sstream>

namespace aethon::storage {
namespace {

bool reached_limit(const RecordSample& sample, const SamplingOptions& options) {
    return options.limit != 0 && sample.refs.size() >= options.limit;
}

void select_ref(RecordSample& sample,
                const SamplingOptions& options,
                const ArchiveCollectionRecordRef& ref,
                std::string reason) {
    SampledRecordRef sampled;
    sampled.ref = ref;
    sampled.reason = std::move(reason);
    sampled.sample_rank = sample.refs.size();
    sample.refs.push_back(std::move(sampled));
    ++sample.selected_count;
    if (options.include_records) {
        auto record = load_collection_record(ref);
        if (record) {
            sample.records.push_back(std::move(*record));
        }
    }
}

bool should_take_hash(const ArchiveCollectionRecordRef& ref, const SamplingOptions& options) {
    auto modulus = options.hash_modulus == 0 ? 1 : options.hash_modulus;
    return deterministic_record_hash(ref) % modulus == options.hash_remainder % modulus;
}

void sample_first_n(RecordSample& sample,
                    const SamplingOptions& options,
                    const std::vector<ArchiveCollectionRecordRef>& refs) {
    for (const auto& ref : refs) {
        ++sample.candidates_seen;
        if (reached_limit(sample, options)) {
            ++sample.rejected_count;
            continue;
        }
        select_ref(sample, options, ref, "first records in query order");
    }
}

void sample_every_nth(RecordSample& sample,
                      const SamplingOptions& options,
                      const std::vector<ArchiveCollectionRecordRef>& refs) {
    auto stride = options.stride == 0 ? 1 : options.stride;
    std::uint64_t index = 0;
    for (const auto& ref : refs) {
        ++sample.candidates_seen;
        bool take = index % stride == 0;
        ++index;
        if (!take || reached_limit(sample, options)) {
            ++sample.rejected_count;
            continue;
        }
        select_ref(sample, options, ref, "stride sample");
    }
}

void sample_time_buckets(RecordSample& sample,
                         const SamplingOptions& options,
                         const std::vector<ArchiveCollectionRecordRef>& refs) {
    auto width = options.bucket_width_ns == 0 ? 1 : options.bucket_width_ns;
    std::map<std::uint64_t, bool> seen_buckets;
    for (const auto& ref : refs) {
        ++sample.candidates_seen;
        auto bucket = ref.capture_time_ns / width;
        if (seen_buckets[bucket] || reached_limit(sample, options)) {
            ++sample.rejected_count;
            continue;
        }
        seen_buckets[bucket] = true;
        select_ref(sample, options, ref, "first record in time bucket");
    }
}

void sample_hash(RecordSample& sample,
                 const SamplingOptions& options,
                 const std::vector<ArchiveCollectionRecordRef>& refs) {
    for (const auto& ref : refs) {
        ++sample.candidates_seen;
        if (!should_take_hash(ref, options) || reached_limit(sample, options)) {
            ++sample.rejected_count;
            continue;
        }
        select_ref(sample, options, ref, "deterministic hash match");
    }
}

void sample_payload_extremes(RecordSample& sample,
                             const SamplingOptions& options,
                             std::vector<ArchiveCollectionRecordRef> refs) {
    std::sort(
        refs.begin(),
        refs.end(),
        [](const ArchiveCollectionRecordRef& left, const ArchiveCollectionRecordRef& right) {
            if (left.payload_size != right.payload_size) {
                return left.payload_size < right.payload_size;
            }
            return left.capture_time_ns < right.capture_time_ns;
        });
    std::size_t low = 0;
    std::size_t high = refs.empty() ? 0 : refs.size() - 1;
    bool take_low = true;
    while (low < refs.size() && low <= high) {
        ++sample.candidates_seen;
        const auto& ref = take_low ? refs[low++] : refs[high--];
        take_low = !take_low;
        if (reached_limit(sample, options)) {
            ++sample.rejected_count;
            continue;
        }
        select_ref(sample, options, ref, "payload size extreme");
        if (high >= refs.size()) {
            break;
        }
    }
}

void normalize_sample(RecordSample& sample) {
    std::stable_sort(
        sample.refs.begin(),
        sample.refs.end(),
        [](const SampledRecordRef& left, const SampledRecordRef& right) {
            return left.ref.capture_time_ns < right.ref.capture_time_ns;
        });
    for (std::uint64_t i = 0; i < sample.refs.size(); ++i) {
        sample.refs[static_cast<std::size_t>(i)].sample_rank = i;
    }
    std::stable_sort(
        sample.records.begin(),
        sample.records.end(),
        [](const ArchiveRecord& left, const ArchiveRecord& right) {
            return left.capture_time_ns < right.capture_time_ns;
        });
}

} // namespace

std::string sampling_mode_name(SamplingMode mode) {
    switch (mode) {
    case SamplingMode::first_n:
        return "first_n";
    case SamplingMode::every_nth:
        return "every_nth";
    case SamplingMode::time_bucket_first:
        return "time_bucket_first";
    case SamplingMode::deterministic_hash:
        return "deterministic_hash";
    case SamplingMode::payload_extremes:
        return "payload_extremes";
    }
    return "unknown";
}

std::uint64_t deterministic_record_hash(const ArchiveCollectionRecordRef& ref) {
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [&](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(ref.archive_ordinal);
    mix(ref.offset);
    mix(ref.capture_time_ns);
    mix(ref.payload_size);
    mix(ref.device);
    mix(ref.sequence);
    for (auto ch : ref.path.string()) {
        mix(static_cast<unsigned char>(ch));
    }
    return hash;
}

RecordSample sample_archive_records(const std::filesystem::path& path,
                                    const SamplingOptions& options,
                                    const ArchiveQuery& query) {
    auto collection = build_archive_collection(std::vector<std::filesystem::path>{path});
    return sample_archive_collection(collection, options, query);
}

RecordSample sample_archive_collection(const ArchiveCollection& collection,
                                       const SamplingOptions& options,
                                       const ArchiveQuery& query) {
    RecordSample sample;
    sample.options = options;
    auto refs = filter_collection_refs(collection, query);
    switch (options.mode) {
    case SamplingMode::first_n:
        sample_first_n(sample, options, refs);
        break;
    case SamplingMode::every_nth:
        sample_every_nth(sample, options, refs);
        break;
    case SamplingMode::time_bucket_first:
        sample_time_buckets(sample, options, refs);
        break;
    case SamplingMode::deterministic_hash:
        sample_hash(sample, options, refs);
        break;
    case SamplingMode::payload_extremes:
        sample_payload_extremes(sample, options, std::move(refs));
        break;
    }
    normalize_sample(sample);
    return sample;
}

std::vector<PayloadDistributionBin> build_payload_distribution(const ArchiveCollection& collection,
                                                              std::uint64_t bin_width) {
    std::map<std::uint64_t, PayloadDistributionBin> bins;
    auto width = bin_width == 0 ? 1 : bin_width;
    for (const auto& ref : collection.records) {
        auto bucket = ref.payload_size / width;
        auto& bin = bins[bucket];
        if (bin.record_count == 0) {
            bin.min_payload_size = bucket * width;
            bin.max_payload_size = bin.min_payload_size + width - 1;
        }
        ++bin.record_count;
    }
    std::vector<PayloadDistributionBin> out;
    out.reserve(bins.size());
    for (const auto& [bucket, bin] : bins) {
        (void)bucket;
        out.push_back(bin);
    }
    return out;
}

std::optional<ArchiveCollectionRecordRef> find_smallest_payload_record(const ArchiveCollection& collection,
                                                                       const ArchiveQuery& query) {
    auto refs = filter_collection_refs(collection, query);
    if (refs.empty()) {
        return std::nullopt;
    }
    return *std::min_element(
        refs.begin(),
        refs.end(),
        [](const ArchiveCollectionRecordRef& left, const ArchiveCollectionRecordRef& right) {
            if (left.payload_size != right.payload_size) {
                return left.payload_size < right.payload_size;
            }
            return left.capture_time_ns < right.capture_time_ns;
        });
}

std::optional<ArchiveCollectionRecordRef> find_largest_payload_record(const ArchiveCollection& collection,
                                                                      const ArchiveQuery& query) {
    auto refs = filter_collection_refs(collection, query);
    if (refs.empty()) {
        return std::nullopt;
    }
    return *std::max_element(
        refs.begin(),
        refs.end(),
        [](const ArchiveCollectionRecordRef& left, const ArchiveCollectionRecordRef& right) {
            if (left.payload_size != right.payload_size) {
                return left.payload_size < right.payload_size;
            }
            return left.capture_time_ns < right.capture_time_ns;
        });
}

std::string render_record_sample(const RecordSample& sample) {
    std::ostringstream out;
    out << "record_sample\n"
        << "  mode: " << sampling_mode_name(sample.options.mode) << "\n"
        << "  candidates_seen: " << sample.candidates_seen << "\n"
        << "  selected_count: " << sample.selected_count << "\n"
        << "  rejected_count: " << sample.rejected_count << "\n";
    for (const auto& sampled : sample.refs) {
        out << "  sample: rank=" << sampled.sample_rank
            << " path=" << sampled.ref.path.string()
            << " offset=" << sampled.ref.offset
            << " capture_time_ns=" << sampled.ref.capture_time_ns
            << " device=" << sampled.ref.device
            << " sequence=" << sampled.ref.sequence
            << " payload_size=" << sampled.ref.payload_size
            << " reason=\"" << sampled.reason << "\"\n";
    }
    return out.str();
}

std::string render_payload_distribution(const std::vector<PayloadDistributionBin>& bins) {
    std::ostringstream out;
    out << "payload_distribution\n";
    for (const auto& bin : bins) {
        out << "  bin: min=" << bin.min_payload_size
            << " max=" << bin.max_payload_size
            << " records=" << bin.record_count
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
