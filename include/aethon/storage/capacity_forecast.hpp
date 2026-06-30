#pragma once

#include "aethon/storage/catalog.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::storage {

struct CapacitySample {
    std::uint64_t time_ns = 0;
    std::uint64_t record_count = 0;
    std::uint64_t payload_bytes = 0;
};

struct CapacityForecastOptions {
    std::uint64_t horizon_ns = 86'400'000'000'000ULL;
    std::uint64_t target_payload_bytes = 10ULL * 1024ULL * 1024ULL * 1024ULL;
};

struct CapacityForecast {
    std::uint64_t current_payload_bytes = 0;
    std::uint64_t projected_payload_bytes = 0;
    double payload_bytes_per_ns = 0.0;
    double records_per_ns = 0.0;
    bool exceeds_target = false;
    std::vector<std::string> notes;
};

[[nodiscard]] std::vector<CapacitySample> samples_from_catalog(const ArchiveCatalog& catalog);
[[nodiscard]] CapacityForecast forecast_capacity(const std::vector<CapacitySample>& samples,
                                                 const CapacityForecastOptions& options = {});
[[nodiscard]] std::string render_capacity_forecast(const CapacityForecast& forecast);

} // namespace aethon::storage
