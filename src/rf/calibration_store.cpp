#include "aethon/rf/calibration_store.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

bool valid_at(const RfCalibrationProfile& profile, std::uint64_t time_ns) {
    if (time_ns < profile.valid_from_ns) {
        return false;
    }
    if (profile.valid_until_ns != 0 && time_ns >= profile.valid_until_ns) {
        return false;
    }
    return true;
}

bool receiver_matches(const RfCalibrationProfile& profile, const std::string& receiver_id) {
    return receiver_id.empty() || profile.receiver_id == receiver_id;
}

bool antenna_matches(const RfCalibrationProfile& profile, const std::string& antenna_id) {
    return antenna_id.empty() || profile.antenna_id.empty() || profile.antenna_id == antenna_id;
}

std::vector<FrequencyCalibrationPoint> sorted_points(std::vector<FrequencyCalibrationPoint> points) {
    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
        return left.frequency_hz < right.frequency_hz;
    });
    return points;
}

FrequencyCalibrationPoint interpolate(const FrequencyCalibrationPoint& left,
                                      const FrequencyCalibrationPoint& right,
                                      std::uint64_t frequency_hz) {
    auto span = static_cast<double>(right.frequency_hz - left.frequency_hz);
    if (span <= 0.0) {
        return left;
    }
    auto position = static_cast<double>(frequency_hz - left.frequency_hz) / span;
    FrequencyCalibrationPoint result;
    result.frequency_hz = frequency_hz;
    result.gain_correction_db = left.gain_correction_db + (right.gain_correction_db - left.gain_correction_db) * position;
    result.noise_correction_db = left.noise_correction_db + (right.noise_correction_db - left.noise_correction_db) * position;
    result.phase_correction_degrees = left.phase_correction_degrees + (right.phase_correction_degrees - left.phase_correction_degrees) * position;
    result.uncertainty_db = std::max(left.uncertainty_db, right.uncertainty_db) + std::abs(position - 0.5) * 0.25;
    return result;
}

double profile_score(const RfCalibrationProfile& profile,
                     std::uint64_t frequency_hz,
                     std::uint64_t time_ns,
                     const std::string& receiver_id,
                     const std::string& antenna_id) {
    if (!receiver_matches(profile, receiver_id) || !antenna_matches(profile, antenna_id)) {
        return -1.0;
    }
    if (!profile.range.contains(frequency_hz) && profile.range.width_hz() > 0) {
        return -1.0;
    }
    double score = 100.0;
    if (!valid_at(profile, time_ns)) {
        score -= 35.0;
    }
    if (profile.points.size() < 2) {
        score -= 15.0;
    }
    switch (profile.source) {
    case CalibrationSource::laboratory:
        score += 10.0;
        break;
    case CalibrationSource::factory:
        score += 5.0;
        break;
    case CalibrationSource::field:
        score += 0.0;
        break;
    case CalibrationSource::derived:
        score -= 5.0;
        break;
    case CalibrationSource::imported:
        score -= 8.0;
        break;
    }
    if (!profile.antenna_id.empty() && !antenna_id.empty() && profile.antenna_id == antenna_id) {
        score += 4.0;
    }
    auto uncertainty = correction_uncertainty_db(profile, frequency_hz);
    score -= std::min(25.0, uncertainty * 4.0);
    return score;
}

std::uint64_t nearest_distance(const RfCalibrationProfile& profile, std::uint64_t frequency_hz) {
    if (profile.points.empty()) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
    for (const auto& point : profile.points) {
        auto distance = point.frequency_hz > frequency_hz
            ? point.frequency_hz - frequency_hz
            : frequency_hz - point.frequency_hz;
        best = std::min(best, distance);
    }
    return best;
}

void add_unique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void add_warning(CalibrationLookup& lookup, std::string warning) {
    lookup.warnings.push_back(std::move(warning));
}

std::vector<telemetry::CalibrationPoint> telemetry_curve(const RfCalibrationProfile& profile, bool noise) {
    std::vector<telemetry::CalibrationPoint> curve;
    curve.reserve(profile.points.size());
    for (const auto& point : sorted_points(profile.points)) {
        telemetry::CalibrationPoint converted;
        converted.raw = static_cast<double>(point.frequency_hz);
        converted.corrected = noise ? point.noise_correction_db : point.gain_correction_db;
        curve.push_back(converted);
    }
    return curve;
}

} // namespace

void CalibrationStore::add_profile(RfCalibrationProfile profile) {
    profile.points = sorted_points(std::move(profile.points));
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const auto& existing) {
        return existing.id == profile.id;
    });
    if (it == profiles_.end()) {
        profiles_.push_back(std::move(profile));
    } else {
        *it = std::move(profile);
    }
    std::sort(profiles_.begin(), profiles_.end(), [](const auto& left, const auto& right) {
        if (left.receiver_id != right.receiver_id) {
            return left.receiver_id < right.receiver_id;
        }
        if (left.range.lower_hz != right.range.lower_hz) {
            return left.range.lower_hz < right.range.lower_hz;
        }
        return left.id < right.id;
    });
}

bool CalibrationStore::remove_profile(const std::string& id) {
    auto before = profiles_.size();
    profiles_.erase(
        std::remove_if(profiles_.begin(), profiles_.end(), [&](const auto& profile) {
            return profile.id == id;
        }),
        profiles_.end());
    return profiles_.size() != before;
}

void CalibrationStore::clear() {
    profiles_.clear();
}

const std::vector<RfCalibrationProfile>& CalibrationStore::profiles() const noexcept {
    return profiles_;
}

std::optional<RfCalibrationProfile> CalibrationStore::find_profile(const std::string& id) const {
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const auto& profile) {
        return profile.id == id;
    });
    if (it == profiles_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<RfCalibrationProfile> CalibrationStore::profiles_for_receiver(const std::string& receiver_id) const {
    std::vector<RfCalibrationProfile> matches;
    for (const auto& profile : profiles_) {
        if (profile.receiver_id == receiver_id) {
            matches.push_back(profile);
        }
    }
    return matches;
}

CalibrationLookup CalibrationStore::lookup(std::uint64_t frequency_hz,
                                           std::uint64_t time_ns,
                                           const std::string& receiver_id,
                                           const std::string& antenna_id) const {
    CalibrationLookup lookup;
    double best_score = -1.0;
    for (const auto& profile : profiles_) {
        auto score = profile_score(profile, frequency_hz, time_ns, receiver_id, antenna_id);
        if (score > best_score) {
            best_score = score;
            lookup.profile = profile;
        }
    }
    if (!lookup.profile) {
        lookup.health = CalibrationHealth::incompatible;
        add_warning(lookup, "no compatible RF calibration profile");
        return lookup;
    }

    lookup.correction = interpolate_correction(*lookup.profile, frequency_hz);
    lookup.health = calibration_health(*lookup.profile, frequency_hz, time_ns);
    lookup.distance_hz = static_cast<double>(nearest_distance(*lookup.profile, frequency_hz));
    if (lookup.health == CalibrationHealth::expired) {
        add_warning(lookup, "RF calibration profile is expired for requested time");
    }
    if (lookup.health == CalibrationHealth::sparse) {
        add_warning(lookup, "RF calibration profile has too few points for robust interpolation");
    }
    if (lookup.health == CalibrationHealth::extrapolated) {
        add_warning(lookup, "RF calibration lookup is outside calibrated point span");
    }
    return lookup;
}

CalibrationStoreSummary CalibrationStore::summarize() const {
    CalibrationStoreSummary summary;
    summary.profiles = profiles_.size();
    for (const auto& profile : profiles_) {
        summary.points += profile.points.size();
        summary.covered_range = merge(summary.covered_range, profile.range);
        add_unique(summary.receiver_ids, profile.receiver_id);
        add_unique(summary.antenna_ids, profile.antenna_id);
    }
    std::sort(summary.receiver_ids.begin(), summary.receiver_ids.end());
    std::sort(summary.antenna_ids.begin(), summary.antenna_ids.end());
    return summary;
}

std::vector<std::string> CalibrationStore::validate() const {
    std::vector<std::string> issues;
    std::set<std::string> ids;
    for (const auto& profile : profiles_) {
        if (profile.id.empty()) {
            issues.push_back("calibration profile has empty id");
        }
        if (!profile.id.empty() && !ids.insert(profile.id).second) {
            issues.push_back("duplicate calibration profile id: " + profile.id);
        }
        if (profile.receiver_id.empty()) {
            issues.push_back("calibration profile " + profile.id + " has empty receiver id");
        }
        if (profile.range.empty()) {
            issues.push_back("calibration profile " + profile.id + " has empty frequency range");
        }
        if (profile.points.empty()) {
            issues.push_back("calibration profile " + profile.id + " has no correction points");
        }
        for (std::size_t i = 0; i < profile.points.size(); ++i) {
            const auto& point = profile.points[i];
            if (!profile.range.contains(point.frequency_hz)) {
                issues.push_back("calibration point outside profile range: " + profile.id);
            }
            if (i > 0 && point.frequency_hz <= profile.points[i - 1].frequency_hz) {
                issues.push_back("calibration points are not strictly increasing: " + profile.id);
            }
            if (point.uncertainty_db < 0.0) {
                issues.push_back("negative calibration uncertainty: " + profile.id);
            }
        }
    }
    return issues;
}

RfCalibrationProfile make_flat_calibration_profile(std::string id,
                                                   std::string receiver_id,
                                                   FrequencyRange range,
                                                   double gain_correction_db,
                                                   double noise_correction_db) {
    RfCalibrationProfile profile;
    profile.id = std::move(id);
    profile.receiver_id = std::move(receiver_id);
    profile.range = range;
    profile.source = CalibrationSource::field;
    profile.points.push_back({range.lower_hz, gain_correction_db, noise_correction_db, 0.0, 0.5});
    if (range.upper_hz > range.lower_hz) {
        profile.points.push_back({range.upper_hz - 1, gain_correction_db, noise_correction_db, 0.0, 0.5});
    }
    return profile;
}

RfCalibrationProfile merge_calibration_profiles(const RfCalibrationProfile& base,
                                                const RfCalibrationProfile& overlay,
                                                std::string merged_id) {
    RfCalibrationProfile merged = base;
    merged.id = std::move(merged_id);
    merged.range = merge(base.range, overlay.range);
    merged.valid_from_ns = std::max(base.valid_from_ns, overlay.valid_from_ns);
    if (base.valid_until_ns == 0) {
        merged.valid_until_ns = overlay.valid_until_ns;
    } else if (overlay.valid_until_ns == 0) {
        merged.valid_until_ns = base.valid_until_ns;
    } else {
        merged.valid_until_ns = std::min(base.valid_until_ns, overlay.valid_until_ns);
    }
    merged.points = base.points;
    for (const auto& point : overlay.points) {
        auto existing = std::find_if(merged.points.begin(), merged.points.end(), [&](const auto& candidate) {
            return candidate.frequency_hz == point.frequency_hz;
        });
        if (existing == merged.points.end()) {
            merged.points.push_back(point);
        } else {
            *existing = point;
        }
    }
    merged.points = sorted_points(std::move(merged.points));
    merged.tags.insert(merged.tags.end(), overlay.tags.begin(), overlay.tags.end());
    std::sort(merged.tags.begin(), merged.tags.end());
    merged.tags.erase(std::unique(merged.tags.begin(), merged.tags.end()), merged.tags.end());
    return merged;
}

SpectrumSlice apply_calibration(const SpectrumSlice& slice,
                                const CalibrationStore& store,
                                std::uint64_t time_ns,
                                const std::string& receiver_id,
                                const std::string& antenna_id) {
    SpectrumSlice corrected = slice;
    for (auto& point : corrected.points) {
        auto lookup = store.lookup(point.frequency_hz, time_ns, receiver_id, antenna_id);
        if (!lookup.profile) {
            continue;
        }
        point.power_dbm += lookup.correction.gain_correction_db;
        point.noise_dbm += lookup.correction.noise_correction_db;
    }
    return corrected;
}

telemetry::CalibrationTable make_telemetry_calibration_table(const CalibrationStore& store,
                                                             const std::string& receiver_id,
                                                             std::uint64_t time_ns) {
    telemetry::CalibrationTable table;
    table.name = "rf_calibration:" + receiver_id;
    std::uint16_t channel = 0;
    for (const auto& profile : store.profiles_for_receiver(receiver_id)) {
        if (!valid_at(profile, time_ns)) {
            continue;
        }
        telemetry::ChannelCalibration gain;
        gain.channel = channel++;
        gain.gain = 1.0;
        gain.offset = 0.0;
        gain.valid_from_ns = profile.valid_from_ns;
        gain.valid_until_ns = profile.valid_until_ns;
        gain.curve = telemetry_curve(profile, false);
        table.channels.push_back(std::move(gain));
    }
    return table;
}

FrequencyCalibrationPoint interpolate_correction(const RfCalibrationProfile& profile,
                                                 std::uint64_t frequency_hz) {
    auto points = sorted_points(profile.points);
    if (points.empty()) {
        return {frequency_hz, 0.0, 0.0, 0.0, 99.0};
    }
    if (points.size() == 1) {
        auto point = points.front();
        point.frequency_hz = frequency_hz;
        point.uncertainty_db += 3.0;
        return point;
    }
    if (frequency_hz <= points.front().frequency_hz) {
        auto result = interpolate(points[0], points[1], points.front().frequency_hz);
        result.frequency_hz = frequency_hz;
        result.uncertainty_db += 2.0;
        return result;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (frequency_hz <= points[i].frequency_hz) {
            return interpolate(points[i - 1], points[i], frequency_hz);
        }
    }
    auto result = interpolate(points[points.size() - 2], points.back(), points.back().frequency_hz);
    result.frequency_hz = frequency_hz;
    result.uncertainty_db += 2.0;
    return result;
}

double correction_uncertainty_db(const RfCalibrationProfile& profile, std::uint64_t frequency_hz) {
    return interpolate_correction(profile, frequency_hz).uncertainty_db;
}

CalibrationHealth calibration_health(const RfCalibrationProfile& profile,
                                     std::uint64_t frequency_hz,
                                     std::uint64_t time_ns) {
    if (!valid_at(profile, time_ns)) {
        return CalibrationHealth::expired;
    }
    if (!profile.range.contains(frequency_hz)) {
        return CalibrationHealth::incompatible;
    }
    if (profile.points.size() < 2) {
        return CalibrationHealth::sparse;
    }
    auto points = sorted_points(profile.points);
    if (frequency_hz < points.front().frequency_hz || frequency_hz > points.back().frequency_hz) {
        return CalibrationHealth::extrapolated;
    }
    return CalibrationHealth::valid;
}

std::string calibration_source_name(CalibrationSource source) {
    switch (source) {
    case CalibrationSource::factory:
        return "factory";
    case CalibrationSource::laboratory:
        return "laboratory";
    case CalibrationSource::field:
        return "field";
    case CalibrationSource::derived:
        return "derived";
    case CalibrationSource::imported:
        return "imported";
    }
    return "field";
}

std::string calibration_health_name(CalibrationHealth health) {
    switch (health) {
    case CalibrationHealth::valid:
        return "valid";
    case CalibrationHealth::expired:
        return "expired";
    case CalibrationHealth::sparse:
        return "sparse";
    case CalibrationHealth::extrapolated:
        return "extrapolated";
    case CalibrationHealth::incompatible:
        return "incompatible";
    }
    return "incompatible";
}

std::string render_calibration_profile(const RfCalibrationProfile& profile) {
    std::ostringstream out;
    out << "rf_calibration_profile id="
        << profile.id
        << " receiver="
        << profile.receiver_id
        << " antenna="
        << profile.antenna_id
        << " range="
        << format_range(profile.range)
        << " source="
        << calibration_source_name(profile.source)
        << " points="
        << profile.points.size()
        << " valid_from_ns="
        << profile.valid_from_ns
        << " valid_until_ns="
        << profile.valid_until_ns
        << "\n";
    for (const auto& point : profile.points) {
        out << "  point "
            << format_frequency(point.frequency_hz)
            << " gain_db="
            << point.gain_correction_db
            << " noise_db="
            << point.noise_correction_db
            << " phase_deg="
            << point.phase_correction_degrees
            << " uncertainty_db="
            << point.uncertainty_db
            << "\n";
    }
    return out.str();
}

std::string render_calibration_lookup(const CalibrationLookup& lookup) {
    std::ostringstream out;
    out << "rf_calibration_lookup health="
        << calibration_health_name(lookup.health)
        << " gain_db="
        << lookup.correction.gain_correction_db
        << " noise_db="
        << lookup.correction.noise_correction_db
        << " phase_deg="
        << lookup.correction.phase_correction_degrees
        << " uncertainty_db="
        << lookup.correction.uncertainty_db
        << " distance_hz="
        << lookup.distance_hz;
    if (lookup.profile) {
        out << " profile=" << lookup.profile->id;
    }
    out << "\n";
    for (const auto& warning : lookup.warnings) {
        out << "  warning: " << warning << "\n";
    }
    return out.str();
}

std::string render_calibration_store_summary(const CalibrationStoreSummary& summary) {
    std::ostringstream out;
    out << "rf_calibration_store profiles="
        << summary.profiles
        << " points="
        << summary.points
        << " covered="
        << format_range(summary.covered_range)
        << "\n";
    for (const auto& receiver : summary.receiver_ids) {
        out << "  receiver: " << receiver << "\n";
    }
    for (const auto& antenna : summary.antenna_ids) {
        out << "  antenna: " << antenna << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
