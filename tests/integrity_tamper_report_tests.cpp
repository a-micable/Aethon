#include "test_harness.hpp"
#include "aethon/integrity/tamper_report.hpp"

AETHON_TEST(tamper_report_summarizes_weighted_samples) {
    aethon::integrity::TamperReport component("tamper_report");
    component.observe({10, 2.0, 1.0, "alpha"});
    component.observe({20, 8.0, 3.0, "beta"});
    auto summary = component.summarize();
    AETHON_REQUIRE(summary.count == 2);
    AETHON_REQUIRE(summary.maximum >= 8.0);
    AETHON_REQUIRE(!component.select(5.0).empty());
}
