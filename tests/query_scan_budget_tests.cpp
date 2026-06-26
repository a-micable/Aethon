#include "test_harness.hpp"
#include "aethon/query/scan_budget.hpp"

AETHON_TEST(scan_budget_evaluates_thresholds) {
    aethon::query::ScanBudget component("scan_budget");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_scan_budget_decision(decision).empty());
}
