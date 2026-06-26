#include "test_harness.hpp"
#include "aethon/observability/export_batch.hpp"

AETHON_TEST(export_batch_evaluates_thresholds) {
    aethon::observability::ExportBatch component("export_batch");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_export_batch_decision(decision).empty());
}
