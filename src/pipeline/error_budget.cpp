#include "aethon/pipeline/error_budget.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::pipeline {
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

ErrorBudget::ErrorBudget(std::string owner) : owner_(std::move(owner)) {}

void ErrorBudget::insert(ErrorBudgetEntry entry) {
    if (entry.key.empty()) {
        entry.key = "default";
    }
    entry.score = normalize_score(entry.score);
    ++key_counts_[entry.key];
    entries_.push_back(std::move(entry));
    if (entries_.size() > 240) {
        auto old_key = entries_.front().key;
        entries_.pop_front();
        auto it = key_counts_.find(old_key);
        if (it != key_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

void ErrorBudget::remove_before(std::uint64_t tick) {
    while (!entries_.empty() && entries_.front().tick < tick) {
        auto old_key = entries_.front().key;
        entries_.pop_front();
        auto it = key_counts_.find(old_key);
        if (it != key_counts_.end() && it->second > 0) {
            --it->second;
        }
    }
}

ErrorBudgetDecision ErrorBudget::evaluate(std::string_view key, double threshold) const {
    ErrorBudgetDecision decision;
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

std::vector<ErrorBudgetEntry> ErrorBudget::drain(double minimum_score) const {
    std::vector<ErrorBudgetEntry> out;
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

std::optional<ErrorBudgetEntry> ErrorBudget::find(std::string_view key) const {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->key == key) {
            return *it;
        }
    }
    return std::nullopt;
}

ErrorBudgetDecision merge_error_budget_decisions(const std::vector<ErrorBudgetDecision>& decisions) {
    ErrorBudgetDecision merged;
    if (decisions.empty()) {
        merged.reason = "no pipeline errors";
        return merged;
    }
    for (const auto& decision : decisions) {
        merged.score += decision.score;
        merged.labels.insert(merged.labels.end(), decision.labels.begin(), decision.labels.end());
        merged.accepted = merged.accepted || decision.accepted;
    }
    merged.score /= static_cast<double>(decisions.size());
    merged.reason = merged.accepted ? "budget tracking" : "pending evidence";
    return merged;
}

std::string render_error_budget_decision(const ErrorBudgetDecision& decision) {
    std::ostringstream out;
    out << "ErrorBudget(accepted=" << (decision.accepted ? "true" : "false")
        << ", score=" << decision.score
        << ", labels=" << decision.labels.size()
        << ", reason=" << decision.reason << ")";
    return out.str();
}

double error_budget_pressure(const ErrorBudget& component) {
    return std::log1p(static_cast<double>(component.size())) / 8.0;
}

} // namespace aethon::pipeline
