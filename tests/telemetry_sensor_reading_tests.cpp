#include "test_harness.hpp"
#include "aethon/telemetry/sensor_reading.hpp"

AETHON_TEST(sensor_reading_evaluates_thresholds) {
    aethon::telemetry::SensorReading component("sensor_reading");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_sensor_reading_decision(decision).empty());
}
