#include "aethon/telemetry/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace aethon::telemetry {
namespace {

double scaled_value(const ScalarReading& reading) {
    auto scale = std::pow(10.0, static_cast<double>(reading.scale));
    return static_cast<double>(reading.value) * scale;
}

ChannelStats* find_channel(std::vector<ChannelStats>& channels, std::uint16_t channel) {
    auto it = std::find_if(
        channels.begin(),
        channels.end(),
        [channel](const ChannelStats& stats) {
            return stats.channel == channel;
        });
    if (it == channels.end()) {
        ChannelStats stats;
        stats.channel = channel;
        channels.push_back(stats);
        return &channels.back();
    }
    return &*it;
}

void sort_channels(std::vector<ChannelStats>& channels) {
    std::sort(
        channels.begin(),
        channels.end(),
        [](const ChannelStats& left, const ChannelStats& right) {
            return left.channel < right.channel;
        });
}

} // namespace

void add_sample(RunningStats& stats, double value) {
    if (stats.count == 0) {
        stats.count = 1;
        stats.min = value;
        stats.max = value;
        stats.mean = value;
        stats.variance = 0.0;
        return;
    }

    ++stats.count;
    stats.min = std::min(stats.min, value);
    stats.max = std::max(stats.max, value);

    auto delta = value - stats.mean;
    stats.mean += delta / static_cast<double>(stats.count);
    auto delta2 = value - stats.mean;
    stats.variance += delta * delta2;
}

double standard_deviation(const RunningStats& stats) {
    if (stats.count < 2) {
        return 0.0;
    }
    auto sample_variance = stats.variance / static_cast<double>(stats.count - 1);
    return std::sqrt(sample_variance);
}

ObservationStats analyze_scalar_observation(const ScalarObservation& observation) {
    ObservationStats stats;
    stats.total_readings = observation.readings.size();
    for (const auto& reading : observation.readings) {
        auto* channel = find_channel(stats.channels, reading.channel);
        add_sample(channel->values, scaled_value(reading));
        if (reading.quality != ReadingQuality::good) {
            ++channel->degraded_readings;
            ++stats.degraded_readings;
        }
    }
    sort_channels(stats.channels);
    return stats;
}

SpectrumStats analyze_spectrum_frame(const SpectrumFrame& frame) {
    SpectrumStats stats;
    for (std::size_t i = 0; i < frame.bins.size(); ++i) {
        auto power = static_cast<double>(frame.bins[i].power_dbm_x10) / 10.0;
        auto noise = static_cast<double>(frame.bins[i].noise_dbm_x10) / 10.0;
        add_sample(stats.power_dbm, power);
        add_sample(stats.noise_dbm, noise);
        if (!stats.strongest_bin || power > static_cast<double>(frame.bins[*stats.strongest_bin].power_dbm_x10) / 10.0) {
            stats.strongest_bin = i;
        }
        if (!stats.weakest_bin || power < static_cast<double>(frame.bins[*stats.weakest_bin].power_dbm_x10) / 10.0) {
            stats.weakest_bin = i;
        }
    }
    return stats;
}

std::string render_running_stats(const RunningStats& stats) {
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(3)
        << "count="
        << stats.count
        << " min="
        << stats.min
        << " max="
        << stats.max
        << " mean="
        << stats.mean
        << " stddev="
        << standard_deviation(stats);
    return out.str();
}

std::string render_observation_stats(const ObservationStats& stats) {
    std::ostringstream out;
    out << "observation_stats\n"
        << "  total_readings: "
        << stats.total_readings
        << "\n"
        << "  degraded_readings: "
        << stats.degraded_readings
        << "\n";
    for (const auto& channel : stats.channels) {
        out << "  channel: "
            << channel.channel
            << " "
            << render_running_stats(channel.values)
            << " degraded="
            << channel.degraded_readings
            << "\n";
    }
    return out.str();
}

std::string render_spectrum_stats(const SpectrumStats& stats) {
    std::ostringstream out;
    out << "spectrum_stats\n"
        << "  power: "
        << render_running_stats(stats.power_dbm)
        << "\n"
        << "  noise: "
        << render_running_stats(stats.noise_dbm)
        << "\n";
    if (stats.strongest_bin) {
        out << "  strongest_bin: "
            << *stats.strongest_bin
            << "\n";
    }
    if (stats.weakest_bin) {
        out << "  weakest_bin: "
            << *stats.weakest_bin
            << "\n";
    }
    return out.str();
}

} // namespace aethon::telemetry
