#include "test_harness.hpp"
#include "aethon/rf/gain_schedule.hpp"

AETHON_TEST(gain_schedule_evaluates_thresholds) {
    aethon::rf::GainSchedule component("gain_schedule");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_gain_schedule_decision(decision).empty());
}
