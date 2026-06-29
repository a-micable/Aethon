#include "aethon/config/schema.hpp"

#include <charconv>
#include <string>
#include <utility>

namespace aethon::config {
namespace {

bool parse_int(std::string_view value, int& out) {
    auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    return result.ec == std::errc() && result.ptr == value.data() + value.size();
}

bool parse_bool(std::string_view value) {
    return value == "true" || value == "false" || value == "1" || value == "0" ||
        value == "yes" || value == "no";
}

void add_issue(ConfigValidation& validation, std::string key, std::string message) {
    validation.issues.push_back(ConfigIssue{std::move(key), std::move(message)});
}

} // namespace

void ConfigSchema::require_string(std::string key) {
    add_rule(std::move(key), ConfigValueType::string, true);
}

void ConfigSchema::optional_string(std::string key) {
    add_rule(std::move(key), ConfigValueType::string, false);
}

void ConfigSchema::require_bool(std::string key) {
    add_rule(std::move(key), ConfigValueType::boolean, true);
}

void ConfigSchema::optional_bool(std::string key) {
    add_rule(std::move(key), ConfigValueType::boolean, false);
}

void ConfigSchema::require_int(std::string key, int min_value, int max_value) {
    add_rule(std::move(key), ConfigValueType::integer, true, min_value, max_value);
}

void ConfigSchema::optional_int(std::string key, int min_value, int max_value) {
    add_rule(std::move(key), ConfigValueType::integer, false, min_value, max_value);
}

void ConfigSchema::add_rule(std::string key, ConfigValueType type, bool required,
                            std::optional<int> min_value, std::optional<int> max_value) {
    rules_.push_back(ConfigRule{
        std::move(key),
        type,
        required,
        min_value,
        max_value,
    });
}

ConfigValidation ConfigSchema::validate(const ParsedConfig& config) const {
    ConfigValidation validation;
    for (const auto& rule : rules_) {
        auto value = config.get(rule.key);
        if (!value) {
            if (rule.required) {
                add_issue(validation, rule.key, "required key is missing");
            }
            continue;
        }
        if (rule.type == ConfigValueType::boolean) {
            if (!parse_bool(*value)) {
                add_issue(validation, rule.key, "value is not a boolean");
            }
            continue;
        }
        if (rule.type == ConfigValueType::integer) {
            int parsed = 0;
            if (!parse_int(*value, parsed)) {
                add_issue(validation, rule.key, "value is not an integer");
                continue;
            }
            if (rule.min_value && parsed < *rule.min_value) {
                add_issue(validation, rule.key, "value is below allowed range");
            }
            if (rule.max_value && parsed > *rule.max_value) {
                add_issue(validation, rule.key, "value is above allowed range");
            }
            continue;
        }
        if (value->empty()) {
            add_issue(validation, rule.key, "string value is empty");
        }
    }
    return validation;
}

ConfigSchema collector_config_schema() {
    ConfigSchema schema;
    schema.require_string("collector.name");
    schema.require_int("collector.region", 0, 65535);
    schema.require_bool("archive.enabled");
    schema.optional_string("archive.path");
    schema.optional_int("stream.max_packet_size", 64, 16 * 1024 * 1024);
    schema.optional_int("routing.default_priority", 0, 255);
    return schema;
}

} // namespace aethon::config
