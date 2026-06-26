#include "test_harness.hpp"
#include "aethon/rf/rssi_tracker.hpp"

AETHON_TEST(rssi_tracker_evaluates_thresholds) {
    aethon::rf::RssiTracker component("rssi_tracker");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_rssi_tracker_decision(decision).empty());
}
