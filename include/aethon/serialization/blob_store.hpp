#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::serialization {

struct BlobStoreSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct BlobStoreSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class BlobStore {
public:
    explicit BlobStore(std::string name = "blob_store");
    void observe(BlobStoreSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] BlobStoreSummary summarize() const;
    [[nodiscard]] std::optional<BlobStoreSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<BlobStoreSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<BlobStoreSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

BlobStoreSummary summarize_blob_store(const std::vector<BlobStoreSample>& samples);
double blob_store_stability_index(const BlobStoreSummary& summary);
std::string describe_blob_store(const BlobStoreSummary& summary);

} // namespace aethon::serialization
