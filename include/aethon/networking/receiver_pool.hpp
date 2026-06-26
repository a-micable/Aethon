#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::networking {

struct ReceiverPoolSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ReceiverPoolSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ReceiverPool {
public:
    explicit ReceiverPool(std::string name = "receiver_pool");
    void observe(ReceiverPoolSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ReceiverPoolSummary summarize() const;
    [[nodiscard]] std::optional<ReceiverPoolSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ReceiverPoolSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ReceiverPoolSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ReceiverPoolSummary summarize_receiver_pool(const std::vector<ReceiverPoolSample>& samples);
double receiver_pool_stability_index(const ReceiverPoolSummary& summary);
std::string describe_receiver_pool(const ReceiverPoolSummary& summary);

} // namespace aethon::networking
