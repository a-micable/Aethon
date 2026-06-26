#include "test_harness.hpp"
#include "aethon/telemetry/capture_label.hpp"

AETHON_TEST(capture_label_evaluates_thresholds) {
    aethon::telemetry::CaptureLabel component("capture_label");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_capture_label_decision(decision).empty());
}
