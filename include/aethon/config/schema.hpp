#pragma once

#include "aethon/config/config_parser.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

enum class ConfigValueType {
    string,
    integer,
    boolean,
};

struct ConfigRule {
    std::string key;
    ConfigValueType type = ConfigValueType::string;
    bool required = false;
    std::optional<int> min_value;
    std::optional<int> max_value;
};

struct ConfigIssue {
    std::string key;
    std::string message;
};

struct ConfigValidation {
    std::vector<ConfigIssue> issues;

    [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

class ConfigSchema {
public:
    void require_string(std::string key);
    void optional_string(std::string key);
    void require_bool(std::string key);
    void optional_bool(std::string key);
    void require_int(std::string key, int min_value, int max_value);
    void optional_int(std::string key, int min_value, int max_value);

    [[nodiscard]] ConfigValidation validate(const ParsedConfig& config) const;
    [[nodiscard]] const std::vector<ConfigRule>& rules() const noexcept { return rules_; }

private:
    void add_rule(std::string key, ConfigValueType type, bool required,
                  std::optional<int> min_value = std::nullopt,
                  std::optional<int> max_value = std::nullopt);

    std::vector<ConfigRule> rules_;
};

ConfigSchema collector_config_schema();

} // namespace aethon::config
