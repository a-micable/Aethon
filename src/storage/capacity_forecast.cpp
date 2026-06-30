#include "aethon/storage/capacity_forecast.hpp"

#include <algorithm>
#include <sstream>

namespace aethon::storage {
namespace {

std::uint64_t manifest_payload(const ArchiveManifest& manifest) {
    std::uint64_t total = 0;
    for (const auto& device : manifest.devices) {
        total += device.total_payload_bytes;
    }
    return total;
}

std::vector<CapacitySample> sorted_samples(std::vector<CapacitySample> samples) {
    std::sort(
        samples.begin(),
        samples.end(),
        [](const CapacitySample& left, const CapacitySample& right) {
            return left.time_ns < right.time_ns;
        });
    return samples;
}

double rate_between(std::uint64_t first_value,
                    std::uint64_t last_value,
                    std::uint64_t first_time,
                    std::uint64_t last_time) {
    if (last_time <= first_time) {
        return 0.0;
    }
    if (last_value < first_value) {
        return 0.0;
    }
    return static_cast<double>(last_value - first_value)
        / static_cast<double>(last_time - first_time);
}

void append_forecast_notes(CapacityForecast& forecast,
                           const CapacityForecastOptions& options,
                           std::size_t sample_count) {
    if (sample_count < 2) {
        forecast.notes.push_back("forecast is based on fewer than two samples");
    }
    if (forecast.payload_bytes_per_ns == 0.0) {
        forecast.notes.push_back("payload growth rate is zero or unavailable");
    }
    if (forecast.exceeds_target) {
        forecast.notes.push_back("projected payload exceeds configured target");
    }
    if (options.horizon_ns == 0) {
        forecast.notes.push_back("forecast horizon is zero");
    }
}

} // namespace

std::vector<CapacitySample> samples_from_catalog(const ArchiveCatalog& catalog) {
    std::vector<CapacitySample> samples;
    samples.reserve(catalog.entries().size());
    std::uint64_t cumulative_records = 0;
    std::uint64_t cumulative_payload = 0;
    for (const auto& entry : catalog.entries()) {
        cumulative_records += entry.manifest.summary.record_count;
        cumulative_payload += manifest_payload(entry.manifest);
        samples.push_back(CapacitySample{
            entry.manifest.summary.last_time_ns,
            cumulative_records,
            cumulative_payload,
        });
    }
    return sorted_samples(std::move(samples));
}

CapacityForecast forecast_capacity(const std::vector<CapacitySample>& input,
                                   const CapacityForecastOptions& options) {
    auto samples = sorted_samples(input);
    CapacityForecast forecast;
    if (samples.empty()) {
        forecast.notes.push_back("no capacity samples available");
        return forecast;
    }

    const auto& first = samples.front();
    const auto& last = samples.back();
    forecast.current_payload_bytes = last.payload_bytes;
    forecast.payload_bytes_per_ns = rate_between(
        first.payload_bytes,
        last.payload_bytes,
        first.time_ns,
        last.time_ns);
    forecast.records_per_ns = rate_between(
        first.record_count,
        last.record_count,
        first.time_ns,
        last.time_ns);

    auto projected_growth = forecast.payload_bytes_per_ns
        * static_cast<double>(options.horizon_ns);
    forecast.projected_payload_bytes = forecast.current_payload_bytes
        + static_cast<std::uint64_t>(projected_growth);
    forecast.exceeds_target = options.target_payload_bytes != 0
        && forecast.projected_payload_bytes > options.target_payload_bytes;

    append_forecast_notes(forecast, options, samples.size());
    return forecast;
}

std::string render_capacity_forecast(const CapacityForecast& forecast) {
    std::ostringstream out;
    out << "capacity_forecast\n"
        << "  current_payload_bytes: "
        << forecast.current_payload_bytes
        << "\n"
        << "  projected_payload_bytes: "
        << forecast.projected_payload_bytes
        << "\n"
        << "  payload_bytes_per_ns: "
        << forecast.payload_bytes_per_ns
        << "\n"
        << "  records_per_ns: "
        << forecast.records_per_ns
        << "\n"
        << "  exceeds_target: "
        << (forecast.exceeds_target ? "yes" : "no")
        << "\n";
    for (const auto& note : forecast.notes) {
        out << "  note: "
            << note
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
