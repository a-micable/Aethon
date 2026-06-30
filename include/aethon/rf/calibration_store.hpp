#pragma once

#include "aethon/rf/types.hpp"
#include "aethon/telemetry/calibration.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class CalibrationSource : std::uint8_t {
    factory = 0,
    laboratory = 1,
    field = 2,
    derived = 3,
    imported = 4,
};

enum class CalibrationHealth : std::uint8_t {
    valid = 0,
    expired = 1,
    sparse = 2,
    extrapolated = 3,
    incompatible = 4,
};

struct FrequencyCalibrationPoint {
    std::uint64_t frequency_hz = 0;
    double gain_correction_db = 0.0;
    double noise_correction_db = 0.0;
    double phase_correction_degrees = 0.0;
    double uncertainty_db = 0.0;
};

struct CalibrationEnvironment {
    double temperature_c = 25.0;
    double humidity_percent = 50.0;
    double reference_level_dbm = -40.0;
    std::string reference_source;
};

struct RfCalibrationProfile {
    std::string id;
    std::string receiver_id;
    std::string antenna_id;
    FrequencyRange range;
    CalibrationSource source = CalibrationSource::field;
    std::uint64_t created_at_ns = 0;
    std::uint64_t valid_from_ns = 0;
    std::uint64_t valid_until_ns = 0;
    CalibrationEnvironment environment;
    std::vector<FrequencyCalibrationPoint> points;
    std::vector<std::string> tags;
};

struct CalibrationLookup {
    std::optional<RfCalibrationProfile> profile;
    FrequencyCalibrationPoint correction;
    CalibrationHealth health = CalibrationHealth::incompatible;
    double distance_hz = 0.0;
    std::vector<std::string> warnings;
};

struct CalibrationStoreSummary {
    std::size_t profiles = 0;
    std::size_t points = 0;
    FrequencyRange covered_range;
    std::vector<std::string> receiver_ids;
    std::vector<std::string> antenna_ids;
};

class CalibrationStore {
public:
    void add_profile(RfCalibrationProfile profile);
    bool remove_profile(const std::string& id);
    void clear();

    [[nodiscard]] const std::vector<RfCalibrationProfile>& profiles() const noexcept;
    [[nodiscard]] std::optional<RfCalibrationProfile> find_profile(const std::string& id) const;
    [[nodiscard]] std::vector<RfCalibrationProfile> profiles_for_receiver(const std::string& receiver_id) const;
    [[nodiscard]] CalibrationLookup lookup(std::uint64_t frequency_hz,
                                           std::uint64_t time_ns,
                                           const std::string& receiver_id = {},
                                           const std::string& antenna_id = {}) const;
    [[nodiscard]] CalibrationStoreSummary summarize() const;
    [[nodiscard]] std::vector<std::string> validate() const;

private:
    std::vector<RfCalibrationProfile> profiles_;
};

[[nodiscard]] RfCalibrationProfile make_flat_calibration_profile(std::string id,
                                                                 std::string receiver_id,
                                                                 FrequencyRange range,
                                                                 double gain_correction_db,
                                                                 double noise_correction_db);
[[nodiscard]] RfCalibrationProfile merge_calibration_profiles(const RfCalibrationProfile& base,
                                                             const RfCalibrationProfile& overlay,
                                                             std::string merged_id);
[[nodiscard]] SpectrumSlice apply_calibration(const SpectrumSlice& slice,
                                             const CalibrationStore& store,
                                             std::uint64_t time_ns,
                                             const std::string& receiver_id = {},
                                             const std::string& antenna_id = {});
[[nodiscard]] telemetry::CalibrationTable make_telemetry_calibration_table(const CalibrationStore& store,
                                                                           const std::string& receiver_id,
                                                                           std::uint64_t time_ns);
[[nodiscard]] FrequencyCalibrationPoint interpolate_correction(const RfCalibrationProfile& profile,
                                                              std::uint64_t frequency_hz);
[[nodiscard]] double correction_uncertainty_db(const RfCalibrationProfile& profile,
                                               std::uint64_t frequency_hz);
[[nodiscard]] CalibrationHealth calibration_health(const RfCalibrationProfile& profile,
                                                   std::uint64_t frequency_hz,
                                                   std::uint64_t time_ns);
[[nodiscard]] std::string calibration_source_name(CalibrationSource source);
[[nodiscard]] std::string calibration_health_name(CalibrationHealth health);
[[nodiscard]] std::string render_calibration_profile(const RfCalibrationProfile& profile);
[[nodiscard]] std::string render_calibration_lookup(const CalibrationLookup& lookup);
[[nodiscard]] std::string render_calibration_store_summary(const CalibrationStoreSummary& summary);

} // namespace aethon::rf
