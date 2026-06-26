#include "test_harness.hpp"
#include "aethon/query/materialized_view.hpp"

AETHON_TEST(materialized_view_evaluates_thresholds) {
    aethon::query::MaterializedView component("materialized_view");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_materialized_view_decision(decision).empty());
}
