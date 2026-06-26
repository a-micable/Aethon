#include "test_harness.hpp"
#include "aethon/query/predicate_plan.hpp"

AETHON_TEST(predicate_plan_evaluates_thresholds) {
    aethon::query::PredicatePlan component("predicate_plan");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_predicate_plan_decision(decision).empty());
}
