#include "aethon/config/config_parser.hpp"

#include "aethon/common/error.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace aethon::config {
namespace {

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

bool valid_key(std::string_view key) {
    if (key.empty() || key.size() > 128) {
        return false;
    }
    for (auto ch : key) {
        auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) && ch != '.' && ch != '_' && ch != '-') {
            return false;
        }
    }
    return true;
}

std::string unquote(std::string value, std::uint32_t line) {
    if (value.size() < 2 || value.front() != '"') {
        return value;
    }
    if (value.back() != '"') {
        throw Error(ErrorCode::malformed_packet, "unterminated quoted config value at line " + std::to_string(line));
    }
    std::string out;
    out.reserve(value.size() - 2);
    bool escaped = false;
    for (std::size_t i = 1; i + 1 < value.size(); ++i) {
        auto ch = value[i];
        if (escaped) {
            switch (ch) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '\\':
            case '"':
                out.push_back(ch);
                break;
            default:
                throw Error(ErrorCode::malformed_packet, "invalid escape in config value");
            }
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else {
            out.push_back(ch);
        }
    }
    if (escaped) {
        throw Error(ErrorCode::malformed_packet, "dangling escape in config value");
    }
    return out;
}

std::string strip_comment(std::string_view line) {
    bool quoted = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        auto ch = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\' && quoted) {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            quoted = !quoted;
            continue;
        }
        if (ch == '#' && !quoted) {
            return std::string(line.substr(0, i));
        }
    }
    return std::string(line);
}

} // namespace

std::optional<std::string> ParsedConfig::get(std::string_view key) const {
    auto it = values.find(key);
    if (it == values.end()) {
        return std::nullopt;
    }
    return it->second;
}

int ParsedConfig::get_int(std::string_view key, int fallback) const {
    auto value = get(key);
    if (!value) {
        return fallback;
    }
    int out = 0;
    auto result = std::from_chars(value->data(), value->data() + value->size(), out);
    if (result.ec != std::errc() || result.ptr != value->data() + value->size()) {
        return fallback;
    }
    return out;
}

bool ParsedConfig::get_bool(std::string_view key, bool fallback) const {
    auto value = get(key);
    if (!value) {
        return fallback;
    }
    return *value == "true" || *value == "1" || *value == "yes";
}

ParsedConfig ConfigParser::parse(std::string_view text) const {
    ParsedConfig config;
    std::size_t start = 0;
    std::uint32_t line_no = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        ++line_no;
        auto raw = text.substr(start, end - start);
        if (!raw.empty() && raw.back() == '\r') {
            raw.remove_suffix(1);
        }
        if (raw.size() > max_line_length_) {
            throw Error(ErrorCode::malformed_packet, "configuration line is too long");
        }
        auto line = trim(strip_comment(raw));
        if (!line.empty()) {
            auto eq = line.find('=');
            if (eq == std::string::npos) {
                throw Error(ErrorCode::malformed_packet, "missing '=' in configuration entry");
            }
            auto key = trim(std::string_view(line).substr(0, eq));
            auto value = trim(std::string_view(line).substr(eq + 1));
            if (!valid_key(key)) {
                throw Error(ErrorCode::malformed_packet, "invalid configuration key");
            }
            if (config.entries.size() >= max_entries_) {
                throw Error(ErrorCode::malformed_packet, "too many configuration entries");
            }
            value = unquote(std::move(value), line_no);
            config.values[key] = value;
            config.entries.push_back(ConfigEntry{std::move(key), std::move(value), line_no});
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }
    return config;
}

ParsedConfig ConfigParser::parse_bytes(std::span<const std::uint8_t> bytes) const {
    std::string text;
    text.reserve(bytes.size());
    for (auto byte : bytes) {
        if (byte == 0) {
            throw Error(ErrorCode::malformed_packet, "configuration contains nul byte");
        }
        text.push_back(static_cast<char>(byte));
    }
    return parse(text);
}

} // namespace aethon::config
