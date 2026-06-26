#include "test_harness.hpp"
#include "aethon/pipeline/error_budget.hpp"

AETHON_TEST(error_budget_evaluates_thresholds) {
    aethon::pipeline::ErrorBudget component("error_budget");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_error_budget_decision(decision).empty());
}
