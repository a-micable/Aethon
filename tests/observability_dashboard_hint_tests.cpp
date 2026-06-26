#include "test_harness.hpp"
#include "aethon/observability/dashboard_hint.hpp"

AETHON_TEST(dashboard_hint_evaluates_thresholds) {
    aethon::observability::DashboardHint component("dashboard_hint");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_dashboard_hint_decision(decision).empty());
}
