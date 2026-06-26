#include "test_harness.hpp"
#include "aethon/control/operator_intent.hpp"

AETHON_TEST(operator_intent_evaluates_thresholds) {
    aethon::control::OperatorIntent component("operator_intent");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_operator_intent_decision(decision).empty());
}
