#include "test_harness.hpp"
#include "aethon/telemetry/clock_anchor.hpp"

AETHON_TEST(clock_anchor_evaluates_thresholds) {
    aethon::telemetry::ClockAnchor component("clock_anchor");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_clock_anchor_decision(decision).empty());
}
