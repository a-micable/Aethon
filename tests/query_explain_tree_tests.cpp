#include "test_harness.hpp"
#include "aethon/query/explain_tree.hpp"

AETHON_TEST(explain_tree_evaluates_thresholds) {
    aethon::query::ExplainTree component("explain_tree");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_explain_tree_decision(decision).empty());
}
