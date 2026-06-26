#include "test_harness.hpp"
#include "aethon/query/filter_cache.hpp"

AETHON_TEST(filter_cache_evaluates_thresholds) {
    aethon::query::FilterCache component("filter_cache");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_filter_cache_decision(decision).empty());
}
