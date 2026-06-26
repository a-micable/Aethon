#include "test_harness.hpp"
#include "aethon/security/token_bucket.hpp"

AETHON_TEST(token_bucket_evaluates_thresholds) {
    aethon::security::TokenBucket component("token_bucket");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_token_bucket_decision(decision).empty());
}
