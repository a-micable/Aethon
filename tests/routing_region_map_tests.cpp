#include "test_harness.hpp"
#include "aethon/routing/region_map.hpp"

AETHON_TEST(region_map_summarizes_weighted_samples) {
    aethon::routing::RegionMap component("region_map");
    component.observe({10, 2.0, 1.0, "alpha"});
    component.observe({20, 8.0, 3.0, "beta"});
    auto summary = component.summarize();
    AETHON_REQUIRE(summary.count == 2);
    AETHON_REQUIRE(summary.maximum >= 8.0);
    AETHON_REQUIRE(!component.select(5.0).empty());
}
AETHON_TEST(region_map_drops_old_samples_1) {
    aethon::routing::RegionMap component("region_map-window-1");
    component.observe({5, 1.0, 1.0, "old"});
    component.observe({50, 4.0, 1.0, "new"});
    component.clear_before(10);
    AETHON_REQUIRE(component.size() == 1);
}
