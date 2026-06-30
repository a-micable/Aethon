#pragma once

#include "aethon/telemetry/payload.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::telemetry {

struct PayloadFilter {
    std::optional<std::uint16_t> channel;
    std::optional<ReadingQuality> quality;
    std::optional<std::int32_t> min_value;
    std::optional<std::int32_t> max_value;
};

[[nodiscard]] std::vector<ScalarReading> filter_readings(const ScalarObservation& observation,
                                                         const PayloadFilter& filter);
[[nodiscard]] bool matches_filter(const ScalarReading& reading, const PayloadFilter& filter);
[[nodiscard]] std::string describe_payload_filter(const PayloadFilter& filter);

} // namespace aethon::telemetry
