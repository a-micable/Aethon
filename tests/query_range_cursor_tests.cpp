#include "test_harness.hpp"
#include "aethon/query/range_cursor.hpp"

AETHON_TEST(range_cursor_evaluates_thresholds) {
    aethon::query::RangeCursor component("range_cursor");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_range_cursor_decision(decision).empty());
}
