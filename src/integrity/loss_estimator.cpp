#include "aethon/integrity/loss_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace aethon::integrity {
namespace {

double bounded_weight(double weight) {
    if (!std::isfinite(weight) || weight <= 0.0) {
        return 1.0;
    }
    return weight > 1000.0 ? 1000.0 : weight;
}

double confidence_from_span(double low, double high, std::size_t count) {
    if (count == 0) {
        return 0.0;
    }
    const double span = std::abs(high - low);
    const double density = std::log1p(static_cast<double>(count));
    return density / (density + span + 1.0);
}

} // namespace

LossEstimator::LossEstimator(std::string name) : name_(std::move(name)) {}

void LossEstimator::observe(LossEstimatorSample sample) {
    sample.weight = bounded_weight(sample.weight);
    if (sample.label.empty()) {
        sample.label = "unlabeled";
    }
    ++label_counts_[sample.label];
    samples_.push_back(std::move(sample));
    if (samples_.size() > 192) {
        auto old = samples_.front().label;
        samples_.pop_front();
        auto it = label_counts_.find(old);
        if (it != label_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

void LossEstimator::clear_before(std::uint64_t timestamp_ns) {
    while (!samples_.empty() && samples_.front().timestamp_ns < timestamp_ns) {
        auto label = samples_.front().label;
        samples_.pop_front();
        auto it = label_counts_.find(label);
        if (it != label_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

LossEstimatorSummary LossEstimator::summarize() const {
    std::vector<LossEstimatorSample> copy(samples_.begin(), samples_.end());
    auto summary = summarize_loss_estimator(copy);
    if (!label_counts_.empty()) {
        auto best = std::max_element(label_counts_.begin(), label_counts_.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        summary.note = name_ + ":" + best->first;
    } else {
        summary.note = name_ + ":empty";
    }
    return summary;
}

std::optional<LossEstimatorSample> LossEstimator::latest(std::string_view label) const {
    for (auto it = samples_.rbegin(); it != samples_.rend(); ++it) {
        if (it->label == label) {
            return *it;
        }
    }
    return std::nullopt;
}

std::vector<LossEstimatorSample> LossEstimator::select(double threshold) const {
    std::vector<LossEstimatorSample> out;
    for (const auto& sample : samples_) {
        if (sample.value >= threshold) {
            out.push_back(sample);
        }
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.timestamp_ns == b.timestamp_ns) {
            return a.label < b.label;
        }
        return a.timestamp_ns < b.timestamp_ns;
    });
    return out;
}

LossEstimatorSummary summarize_loss_estimator(const std::vector<LossEstimatorSample>& samples) {
    LossEstimatorSummary summary;
    summary.count = samples.size();
    if (samples.empty()) {
        summary.note = "no stream gaps observed";
        return summary;
    }
    summary.minimum = std::numeric_limits<double>::infinity();
    summary.maximum = -std::numeric_limits<double>::infinity();
    double weighted_sum = 0.0;
    double total_weight = 0.0;
    for (const auto& sample : samples) {
        summary.minimum = std::min(summary.minimum, sample.value);
        summary.maximum = std::max(summary.maximum, sample.value);
        const auto weight = bounded_weight(sample.weight);
        weighted_sum += sample.value * weight;
        total_weight += weight;
    }
    summary.average = total_weight == 0.0 ? 0.0 : weighted_sum / total_weight;
    summary.confidence = confidence_from_span(summary.minimum, summary.maximum, summary.count);
    summary.note = "loss rate estimation";
    return summary;
}

double loss_estimator_stability_index(const LossEstimatorSummary& summary) {
    if (summary.count == 0) {
        return 0.0;
    }
    const double spread = std::abs(summary.maximum - summary.minimum);
    const double center = std::abs(summary.average) + 1.0;
    return summary.confidence / (1.0 + spread / center);
}

std::string describe_loss_estimator(const LossEstimatorSummary& summary) {
    std::ostringstream out;
    out << "LossEstimator[count=" << summary.count
        << ", load=" << summary.average
        << ", confidence=" << summary.confidence
        << ", note=" << summary.note << "]";
    return out.str();
}

} // namespace aethon::integrity
