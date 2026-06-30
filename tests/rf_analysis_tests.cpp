#include "test_harness.hpp"

#include "aethon/rf/band_plan.hpp"
#include "aethon/rf/calibration_store.hpp"
#include "aethon/rf/interference.hpp"
#include "aethon/rf/iq_summary.hpp"
#include "aethon/rf/signal_quality.hpp"
#include "aethon/rf/spectral_analysis.hpp"
#include "aethon/rf/spectrum_history.hpp"
#include "aethon/rf/sweep_planner.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

aethon::telemetry::SpectrumFrame make_test_frame() {
    aethon::telemetry::SpectrumFrame frame;
    frame.center_frequency_hz = 433'920'000;
    frame.span_hz = 2'000'000;
    frame.bin_width_hz = 10'000;
    frame.bins.resize(200);
    for (std::size_t i = 0; i < frame.bins.size(); ++i) {
        frame.bins[i].power_dbm_x10 = -980;
        frame.bins[i].noise_dbm_x10 = -1040;
    }
    frame.bins[91].power_dbm_x10 = -720;
    frame.bins[92].power_dbm_x10 = -620;
    frame.bins[93].power_dbm_x10 = -710;
    frame.bins[130].power_dbm_x10 = -700;
    return frame;
}

aethon::rf::IqWindow make_iq_tone() {
    aethon::rf::IqWindow window;
    window.sample_rate_hz = 48'000;
    window.center_frequency_hz = 433'920'000;
    constexpr double pi = 3.141592653589793238462643383279502884;
    for (std::size_t i = 0; i < 256; ++i) {
        auto phase = 2.0 * pi * 1'000.0 * static_cast<double>(i) / static_cast<double>(window.sample_rate_hz);
        window.samples.push_back({0.5 * std::cos(phase) + 0.02, 0.45 * std::sin(phase) - 0.01});
    }
    return window;
}

} // namespace

AETHON_TEST(rf_band_plan_queries_allocations_and_channels) {
    auto plan = aethon::rf::make_monitoring_band_plan();

    auto allocations = plan.allocations_at(433'920'000);
    AETHON_REQUIRE(!allocations.empty());

    auto nearest = plan.nearest_channel(433'925'000, aethon::rf::RfService::ism);
    AETHON_REQUIRE(nearest.has_value());
    AETHON_REQUIRE(nearest->center_hz >= 433'000'000);

    aethon::rf::BandPlanQuery query;
    query.service = aethon::rf::RfService::cellular;
    auto cellular = plan.query(query);
    AETHON_REQUIRE(cellular.size() >= 2);
}

AETHON_TEST(rf_spectral_analysis_detects_peaks_and_noise_floor) {
    auto frame = make_test_frame();
    aethon::rf::PeakDetectorConfig config;
    config.min_prominence_db = 10.0;
    config.min_snr_db = 8.0;
    config.merge_gap_hz = 20'000;

    auto analysis = aethon::rf::analyze_spectrum_frame(frame, config);
    AETHON_REQUIRE(!analysis.peaks.empty());
    AETHON_REQUIRE(analysis.noise.samples == frame.bins.size());
    AETHON_REQUIRE(analysis.occupancy.occupied_ratio > 0.0);
    AETHON_REQUIRE(analysis.peaks.front().prominence_db > 20.0);

    auto rendered = aethon::rf::render_spectral_analysis(analysis);
    AETHON_REQUIRE(rendered.find("spectral_analysis") != std::string::npos);
}

AETHON_TEST(rf_calibration_store_interpolates_and_corrects_spectrum) {
    aethon::rf::CalibrationStore store;
    auto profile = aethon::rf::make_flat_calibration_profile(
        "rx1-433",
        "rx1",
        {433'000'000, 435'000'000},
        2.0,
        -1.0);
    profile.valid_from_ns = 10;
    profile.valid_until_ns = 1'000;
    profile.points.push_back({434'000'000, 4.0, -2.0, 1.0, 0.25});
    store.add_profile(profile);

    auto lookup = store.lookup(434'000'000, 100, "rx1");
    AETHON_REQUIRE(lookup.profile.has_value());
    AETHON_REQUIRE(lookup.health == aethon::rf::CalibrationHealth::valid);
    AETHON_REQUIRE(lookup.correction.gain_correction_db > 3.0);

    auto slice = aethon::rf::spectrum_slice_from_frame(make_test_frame());
    auto corrected = aethon::rf::apply_calibration(slice, store, 100, "rx1");
    AETHON_REQUIRE(corrected.points[20].power_dbm > slice.points[20].power_dbm);

    auto table = aethon::rf::make_telemetry_calibration_table(store, "rx1", 100);
    AETHON_REQUIRE(!table.channels.empty());
    AETHON_REQUIRE(store.validate().empty());
}

AETHON_TEST(rf_interference_classifier_flags_unexpected_carriers) {
    auto plan = aethon::rf::make_monitoring_band_plan();
    auto analysis = aethon::rf::analyze_spectrum_frame(make_test_frame());
    auto report = aethon::rf::classify_interference(analysis, plan);

    AETHON_REQUIRE(!report.findings.empty());
    AETHON_REQUIRE(report.worst_confidence > 0.0);

    auto rendered = aethon::rf::render_interference_report(report);
    AETHON_REQUIRE(rendered.find("interference_report") != std::string::npos);
}

AETHON_TEST(rf_sweep_planner_builds_channel_and_followup_segments) {
    auto plan = aethon::rf::make_monitoring_band_plan();
    aethon::rf::ReceiverProfile receiver;
    receiver.name = "test_rx";
    receiver.min_frequency_hz = 100'000'000;
    receiver.max_frequency_hz = 1'800'000'000;
    receiver.max_span_hz = 1'000'000;
    receiver.usable_fraction = 0.75;

    aethon::rf::SweepRequest request;
    request.range = {433'000'000, 434'000'000};
    request.service = aethon::rf::RfService::ism;
    request.resolution_hz = 5'000;
    request.dwell_us = 10'000;
    request.priority = aethon::rf::SweepPriority::high;

    auto sweep = aethon::rf::plan_sweep(plan, receiver, {request});
    AETHON_REQUIRE(!sweep.segments.empty());
    AETHON_REQUIRE(sweep.estimated_duration_us > 0);
    AETHON_REQUIRE(sweep.covered_hz > 0);

    auto report = aethon::rf::classify_interference(aethon::rf::analyze_spectrum_frame(make_test_frame()), plan);
    auto followup = aethon::rf::plan_interference_followup(plan, receiver, report);
    AETHON_REQUIRE(!followup.segments.empty());
}

AETHON_TEST(rf_iq_summary_reports_receiver_health_indicators) {
    auto window = make_iq_tone();
    auto summary = aethon::rf::summarize_iq(window);

    AETHON_REQUIRE(summary.sample_count == window.samples.size());
    AETHON_REQUIRE(summary.rms > 0.1);
    AETHON_REQUIRE(summary.dc_offset_magnitude > 0.0);
    AETHON_REQUIRE(std::abs(summary.estimated_frequency_offset_hz) > 500.0);

    auto segments = aethon::rf::summarize_iq_segments(window, 64);
    AETHON_REQUIRE(segments.size() == 4);
}

AETHON_TEST(rf_signal_quality_combines_spectrum_interference_and_iq) {
    auto plan = aethon::rf::make_monitoring_band_plan();
    auto analysis = aethon::rf::analyze_spectrum_frame(make_test_frame());
    auto report = aethon::rf::classify_interference(analysis, plan);
    auto iq = aethon::rf::summarize_iq(make_iq_tone());

    aethon::rf::SignalQualityInput input;
    input.spectral = analysis;
    input.interference = report;
    input.iq = iq;
    input.allocation = plan.best_allocation(analysis.slice.center_frequency_hz);

    auto quality = aethon::rf::score_signal_quality(input);
    AETHON_REQUIRE(quality.score >= 0.0);
    AETHON_REQUIRE(quality.score <= 100.0);
    AETHON_REQUIRE(!quality.findings.empty());

    auto rendered = aethon::rf::render_signal_quality(quality);
    AETHON_REQUIRE(rendered.find("signal_quality") != std::string::npos);
}

AETHON_TEST(rf_spectrum_history_tracks_persistent_peaks) {
    auto plan = aethon::rf::make_monitoring_band_plan();
    auto frame1 = make_test_frame();
    auto frame2 = make_test_frame();
    frame2.bins[92].power_dbm_x10 = -560;

    aethon::rf::SpectrumHistory history;
    history.add(aethon::rf::make_spectrum_observation(1'000, frame1, plan));
    history.add(aethon::rf::make_spectrum_observation(3'600'000'001'000ULL, frame2, plan));

    auto report = history.analyze(25'000);
    AETHON_REQUIRE(history.size() == 2);
    AETHON_REQUIRE(!report.tracks.empty());
    AETHON_REQUIRE(report.mean_quality_score >= 0.0);

    auto rendered = aethon::rf::render_spectrum_history_report(report);
    AETHON_REQUIRE(rendered.find("spectrum_history") != std::string::npos);
}
