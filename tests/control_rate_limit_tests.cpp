#include "test_harness.hpp"
#include "aethon/control/rate_limit.hpp"

AETHON_TEST(rate_limit_evaluates_thresholds) {
    aethon::control::RateLimit component("rate_limit");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_rate_limit_decision(decision).empty());
}
