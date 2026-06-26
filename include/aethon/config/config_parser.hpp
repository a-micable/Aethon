#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

struct ConfigEntry {
    std::string key;
    std::string value;
    std::uint32_t line = 0;
};

struct ParsedConfig {
    std::vector<ConfigEntry> entries;
    std::map<std::string, std::string, std::less<>> values;

    [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
    [[nodiscard]] int get_int(std::string_view key, int fallback) const;
    [[nodiscard]] bool get_bool(std::string_view key, bool fallback) const;
};

class ConfigParser {
public:
    void set_max_line_length(std::size_t value) noexcept { max_line_length_ = value; }
    void set_max_entries(std::size_t value) noexcept { max_entries_ = value; }

    [[nodiscard]] ParsedConfig parse(std::string_view text) const;
    [[nodiscard]] ParsedConfig parse_bytes(std::span<const std::uint8_t> bytes) const;

private:
    std::size_t max_line_length_ = 8192;
    std::size_t max_entries_ = 4096;
};

} // namespace aethon::config
