#include "test_harness.hpp"
#include "aethon/telemetry/payload_digest.hpp"

AETHON_TEST(payload_digest_evaluates_thresholds) {
    aethon::telemetry::PayloadDigest component("payload_digest");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_payload_digest_decision(decision).empty());
}
