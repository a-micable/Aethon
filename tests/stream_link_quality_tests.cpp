#include "test_harness.hpp"
#include "aethon/stream/link_quality.hpp"

AETHON_TEST(link_quality_summarizes_weighted_samples) {
    aethon::stream::LinkQuality component("link_quality");
    component.observe({10, 2.0, 1.0, "alpha"});
    component.observe({20, 8.0, 3.0, "beta"});
    auto summary = component.summarize();
    AETHON_REQUIRE(summary.count == 2);
    AETHON_REQUIRE(summary.maximum >= 8.0);
    AETHON_REQUIRE(!component.select(5.0).empty());
}
