#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct SignatureNoteEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SignatureNoteDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SignatureNote {
public:
    explicit SignatureNote(std::string owner = "signature_note");
    void insert(SignatureNoteEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SignatureNoteDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SignatureNoteEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SignatureNoteEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SignatureNoteEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SignatureNoteDecision merge_signature_note_decisions(const std::vector<SignatureNoteDecision>& decisions);
std::string render_signature_note_decision(const SignatureNoteDecision& decision);
double signature_note_pressure(const SignatureNote& component);

} // namespace aethon::security
