#include "test_harness.hpp"
#include "aethon/control/safety_check.hpp"

AETHON_TEST(safety_check_evaluates_thresholds) {
    aethon::control::SafetyCheck component("safety_check");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_safety_check_decision(decision).empty());
}
