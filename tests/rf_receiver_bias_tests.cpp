#include "test_harness.hpp"
#include "aethon/rf/receiver_bias.hpp"

AETHON_TEST(receiver_bias_evaluates_thresholds) {
    aethon::rf::ReceiverBias component("receiver_bias");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_receiver_bias_decision(decision).empty());
}
