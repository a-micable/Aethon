#include "aethon/telemetry/filter.hpp"

#include <sstream>

namespace aethon::telemetry {

std::vector<ScalarReading> filter_readings(const ScalarObservation& observation,
                                           const PayloadFilter& filter) {
    std::vector<ScalarReading> readings;
    for (const auto& reading : observation.readings) {
        if (matches_filter(reading, filter)) {
            readings.push_back(reading);
        }
    }
    return readings;
}

bool matches_filter(const ScalarReading& reading, const PayloadFilter& filter) {
    if (filter.channel && reading.channel != *filter.channel) {
        return false;
    }
    if (filter.quality && reading.quality != *filter.quality) {
        return false;
    }
    if (filter.min_value && reading.value < *filter.min_value) {
        return false;
    }
    if (filter.max_value && reading.value > *filter.max_value) {
        return false;
    }
    return true;
}

std::string describe_payload_filter(const PayloadFilter& filter) {
    std::ostringstream out;
    out << "payload_filter";
    if (filter.channel) {
        out << " channel=" << *filter.channel;
    }
    if (filter.quality) {
        out << " quality=" << reading_quality_name(*filter.quality);
    }
    if (filter.min_value) {
        out << " min_value=" << *filter.min_value;
    }
    if (filter.max_value) {
        out << " max_value=" << *filter.max_value;
    }
    return out.str();
}

} // namespace aethon::telemetry
