#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::routing {

struct RouteTableSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RouteTableSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RouteTable {
public:
    explicit RouteTable(std::string name = "route_table");
    void observe(RouteTableSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RouteTableSummary summarize() const;
    [[nodiscard]] std::optional<RouteTableSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RouteTableSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RouteTableSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RouteTableSummary summarize_route_table(const std::vector<RouteTableSample>& samples);
double route_table_stability_index(const RouteTableSummary& summary);
std::string describe_route_table(const RouteTableSummary& summary);

} // namespace aethon::routing
