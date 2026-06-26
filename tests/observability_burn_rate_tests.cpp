#include "test_harness.hpp"
#include "aethon/observability/burn_rate.hpp"

AETHON_TEST(burn_rate_evaluates_thresholds) {
    aethon::observability::BurnRate component("burn_rate");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_burn_rate_decision(decision).empty());
}
