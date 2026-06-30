#include "aethon/storage/query_parser.hpp"

#include "aethon/common/error.hpp"

#include <charconv>
#include <cctype>
#include <optional>
#include <sstream>
#include <utility>

namespace aethon::storage {
namespace {

bool is_space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::string lower(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::optional<std::uint64_t> parse_u64(std::string_view value) {
    std::uint64_t out = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    if (result.ec != std::errc() || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return out;
}

std::optional<std::size_t> parse_size(std::string_view value) {
    auto parsed = parse_u64(value);
    if (!parsed) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(*parsed);
}

std::optional<protocol::PacketKind> parse_kind(std::string_view value) {
    auto name = lower(value);
    if (name == "heartbeat" || name == "1") {
        return protocol::PacketKind::heartbeat;
    }
    if (name == "observation" || name == "2") {
        return protocol::PacketKind::observation;
    }
    if (name == "spectrum" || name == "3") {
        return protocol::PacketKind::spectrum;
    }
    if (name == "control" || name == "4") {
        return protocol::PacketKind::control;
    }
    if (name == "capabilities" || name == "5") {
        return protocol::PacketKind::capabilities;
    }
    return std::nullopt;
}

void add_diagnostic(ParsedArchiveQuery& parsed, std::string token, std::string message) {
    parsed.diagnostics.push_back(QueryParseDiagnostic{
        std::move(token),
        std::move(message),
    });
}

bool parse_quoted(std::string_view expression, std::size_t& pos, std::string& out) {
    if (pos >= expression.size() || expression[pos] != '"') {
        return false;
    }
    ++pos;
    while (pos < expression.size()) {
        char ch = expression[pos++];
        if (ch == '"') {
            return true;
        }
        if (ch == '\\' && pos < expression.size()) {
            char escaped = expression[pos++];
            switch (escaped) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '\\':
            case '"':
                out.push_back(escaped);
                break;
            default:
                out.push_back(escaped);
                break;
            }
        } else {
            out.push_back(ch);
        }
    }
    return false;
}

std::string parse_bare(std::string_view expression, std::size_t& pos) {
    std::string out;
    while (pos < expression.size() && !is_space(expression[pos])) {
        out.push_back(expression[pos++]);
    }
    return out;
}

std::string parse_word(std::string_view expression, std::size_t& pos) {
    std::string out;
    while (pos < expression.size() && !is_space(expression[pos]) && expression[pos] != '=') {
        out.push_back(expression[pos++]);
    }
    return out;
}

void apply_token(ParsedArchiveQuery& parsed, const QueryToken& token) {
    auto key = lower(token.key);
    if (key == "start" || key == "start_time" || key == "start_time_ns") {
        auto value = parse_u64(token.value);
        if (value) {
            parsed.query.start_time_ns = *value;
        } else {
            add_diagnostic(parsed, token.key, "start time must be an unsigned integer");
        }
        return;
    }
    if (key == "end" || key == "end_time" || key == "end_time_ns") {
        auto value = parse_u64(token.value);
        if (value) {
            parsed.query.end_time_ns = *value;
        } else {
            add_diagnostic(parsed, token.key, "end time must be an unsigned integer");
        }
        return;
    }
    if (key == "device" || key == "device_id") {
        auto value = parse_u64(token.value);
        if (value) {
            parsed.query.device = *value;
        } else {
            add_diagnostic(parsed, token.key, "device must be an unsigned integer");
        }
        return;
    }
    if (key == "kind" || key == "packet_kind") {
        auto value = parse_kind(token.value);
        if (value) {
            parsed.query.kind = *value;
        } else {
            add_diagnostic(parsed, token.key, "packet kind is not recognized");
        }
        return;
    }
    if (key == "min_payload" || key == "min_payload_size") {
        auto value = parse_size(token.value);
        if (value) {
            parsed.query.min_payload_size = *value;
        } else {
            add_diagnostic(parsed, token.key, "minimum payload size must be an unsigned integer");
        }
        return;
    }
    if (key == "max_payload" || key == "max_payload_size") {
        auto value = parse_size(token.value);
        if (value) {
            parsed.query.max_payload_size = *value;
        } else {
            add_diagnostic(parsed, token.key, "maximum payload size must be an unsigned integer");
        }
        return;
    }
    if (key == "limit") {
        auto value = parse_size(token.value);
        if (value) {
            parsed.query.limit = *value;
        } else {
            add_diagnostic(parsed, token.key, "limit must be an unsigned integer");
        }
        return;
    }
    add_diagnostic(parsed, token.key, "unknown query key");
}

void validate_query_ranges(ParsedArchiveQuery& parsed) {
    if (parsed.query.start_time_ns && parsed.query.end_time_ns
        && *parsed.query.start_time_ns > *parsed.query.end_time_ns) {
        add_diagnostic(parsed, "time", "start time is after end time");
    }
    if (parsed.query.min_payload_size && parsed.query.max_payload_size
        && *parsed.query.min_payload_size > *parsed.query.max_payload_size) {
        add_diagnostic(parsed, "payload", "minimum payload size exceeds maximum payload size");
    }
}

} // namespace

std::vector<QueryToken> tokenize_query(std::string_view expression) {
    std::vector<QueryToken> tokens;
    std::size_t pos = 0;
    while (pos < expression.size()) {
        while (pos < expression.size() && is_space(expression[pos])) {
            ++pos;
        }
        if (pos >= expression.size()) {
            break;
        }
        auto key = parse_word(expression, pos);
        while (pos < expression.size() && is_space(expression[pos])) {
            ++pos;
        }
        if (pos >= expression.size() || expression[pos] != '=') {
            tokens.push_back(QueryToken{key, ""});
            continue;
        }
        ++pos;
        while (pos < expression.size() && is_space(expression[pos])) {
            ++pos;
        }
        std::string value;
        if (pos < expression.size() && expression[pos] == '"') {
            if (!parse_quoted(expression, pos, value)) {
                value.clear();
            }
        } else {
            value = parse_bare(expression, pos);
        }
        tokens.push_back(QueryToken{key, value});
    }
    return tokens;
}

ParsedArchiveQuery parse_archive_query(std::string_view expression) {
    ParsedArchiveQuery parsed;
    parsed.tokens = tokenize_query(expression);
    for (const auto& token : parsed.tokens) {
        if (token.key.empty()) {
            add_diagnostic(parsed, token.key, "query key is empty");
            continue;
        }
        if (token.value.empty()) {
            add_diagnostic(parsed, token.key, "query value is empty");
            continue;
        }
        apply_token(parsed, token);
    }
    validate_query_ranges(parsed);
    return parsed;
}

std::string render_query_diagnostics(const ParsedArchiveQuery& parsed) {
    std::ostringstream out;
    if (parsed.diagnostics.empty()) {
        out << "query diagnostics: ok\n";
        return out.str();
    }
    out << "query diagnostics:\n";
    for (const auto& diagnostic : parsed.diagnostics) {
        out << "  "
            << diagnostic.token
            << ": "
            << diagnostic.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::storage
