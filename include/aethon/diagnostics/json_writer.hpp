#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct JsonField {
    std::string name;
    std::string value;
    bool raw = false;
};

class JsonObject {
public:
    void string(std::string name, std::string value);
    void number(std::string name, std::uint64_t value);
    void boolean(std::string name, bool value);
    void raw(std::string name, std::string value);

    [[nodiscard]] const std::vector<JsonField>& fields() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t field_count() const noexcept;

private:
    std::vector<JsonField> fields_;
};

[[nodiscard]] std::string json_escape(std::string_view value);
[[nodiscard]] std::string render_json_object(const JsonObject& object);
[[nodiscard]] std::string render_json_object_pretty(const JsonObject& object, unsigned indent = 2);
[[nodiscard]] std::string render_json_array(const std::vector<JsonObject>& objects);
[[nodiscard]] std::string render_json_string_array(const std::vector<std::string>& values);
[[nodiscard]] JsonObject make_status_object(std::string status, std::string message);
[[nodiscard]] std::string render_status_json(std::string status,
                                             std::string message,
                                             const std::vector<std::string>& notes = {});
[[nodiscard]] bool is_json_literal(std::string_view value);
[[nodiscard]] JsonObject make_count_object(std::string name, std::uint64_t value);
[[nodiscard]] JsonObject make_error_object(std::string code, std::string message);

} // namespace aethon::diagnostics
