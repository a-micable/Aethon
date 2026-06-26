#include "test_harness.hpp"
#include "aethon/observability/alert_rule.hpp"

AETHON_TEST(alert_rule_evaluates_thresholds) {
    aethon::observability::AlertRule component("alert_rule");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_alert_rule_decision(decision).empty());
}
