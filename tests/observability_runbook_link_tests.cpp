#include "test_harness.hpp"
#include "aethon/observability/runbook_link.hpp"

AETHON_TEST(runbook_link_evaluates_thresholds) {
    aethon::observability::RunbookLink component("runbook_link");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_runbook_link_decision(decision).empty());
}
