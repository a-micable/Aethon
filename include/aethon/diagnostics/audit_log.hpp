#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct AuditLogSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct AuditLogSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class AuditLog {
public:
    explicit AuditLog(std::string name = "audit_log");
    void observe(AuditLogSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] AuditLogSummary summarize() const;
    [[nodiscard]] std::optional<AuditLogSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<AuditLogSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<AuditLogSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

AuditLogSummary summarize_audit_log(const std::vector<AuditLogSample>& samples);
double audit_log_stability_index(const AuditLogSummary& summary);
std::string describe_audit_log(const AuditLogSummary& summary);

} // namespace aethon::diagnostics
