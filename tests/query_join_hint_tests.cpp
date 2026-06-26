#include "test_harness.hpp"
#include "aethon/query/join_hint.hpp"

AETHON_TEST(join_hint_evaluates_thresholds) {
    aethon::query::JoinHint component("join_hint");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_join_hint_decision(decision).empty());
}
