#include "aethon/telemetry/event_window.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::telemetry {
namespace {

double normalize_score(double score) {
    if (!std::isfinite(score)) {
        return 0.0;
    }
    if (score < 0.0) {
        return 0.0;
    }
    return score > 1.0 ? 1.0 : score;
}

std::string choose_reason(bool accepted, std::string_view key) {
    if (accepted) {
        return std::string(key) + ":accepted";
    }
    return std::string(key) + ":below-threshold";
}

} // namespace

EventWindow::EventWindow(std::string owner) : owner_(std::move(owner)) {}

void EventWindow::insert(EventWindowEntry entry) {
    if (entry.key.empty()) {
        entry.key = "default";
    }
    entry.score = normalize_score(entry.score);
    ++key_counts_[entry.key];
    entries_.push_back(std::move(entry));
    if (entries_.size() > 192) {
        auto old_key = entries_.front().key;
        entries_.pop_front();
        auto it = key_counts_.find(old_key);
        if (it != key_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

void EventWindow::remove_before(std::uint64_t tick) {
    while (!entries_.empty() && entries_.front().tick < tick) {
        auto old_key = entries_.front().key;
        entries_.pop_front();
        auto it = key_counts_.find(old_key);
        if (it != key_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

EventWindowDecision EventWindow::evaluate(std::string_view key, double threshold) const {
    EventWindowDecision decision;
    double total = 0.0;
    std::size_t matched = 0;
    for (const auto& entry : entries_) {
        if (entry.key == key) {
            total += entry.score;
            ++matched;
            if (!entry.detail.empty()) {
                decision.labels.push_back(entry.detail);
            }
        }
    }
    decision.score = matched == 0 ? 0.0 : total / static_cast<double>(matched);
    decision.accepted = decision.score >= threshold;
    decision.reason = choose_reason(decision.accepted, key);
    return decision;
}

std::vector<EventWindowEntry> EventWindow::drain(double minimum_score) const {
    std::vector<EventWindowEntry> out;
    for (const auto& entry : entries_) {
        if (entry.score >= minimum_score) {
            out.push_back(entry);
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.tick == b.tick) {
            return a.ordinal < b.ordinal;
        }
        return a.tick < b.tick;
    });
    return out;
}

std::optional<EventWindowEntry> EventWindow::find(std::string_view key) const {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->key == key) {
            return *it;
        }
    }
    return std::nullopt;
}

EventWindowDecision merge_event_window_decisions(const std::vector<EventWindowDecision>& decisions) {
    EventWindowDecision merged;
    if (decisions.empty()) {
        merged.reason = "no telemetry events";
        return merged;
    }
    for (const auto& decision : decisions) {
        merged.score += decision.score;
        merged.labels.insert(merged.labels.end(), decision.labels.begin(), decision.labels.end());
        merged.accepted = merged.accepted || decision.accepted;
    }
    merged.score /= static_cast<double>(decisions.size());
    merged.reason = merged.accepted ? "window summaries" : "pending evidence";
    return merged;
}

std::string render_event_window_decision(const EventWindowDecision& decision) {
    std::ostringstream out;
    out << "EventWindow(accepted=" << (decision.accepted ? "true" : "false")
        << ", score=" << decision.score
        << ", labels=" << decision.labels.size()
        << ", reason=" << decision.reason << ")";
    return out.str();
}

double event_window_pressure(const EventWindow& component) {
    return std::log1p(static_cast<double>(component.size())) / 8.0;
}

} // namespace aethon::telemetry
