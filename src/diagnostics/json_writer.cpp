#include "aethon/diagnostics/json_writer.hpp"

#include <iomanip>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

std::string hex_escape(unsigned char ch) {
    std::ostringstream out;
    out << "\\u"
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << static_cast<unsigned>(ch);
    return out.str();
}

void append_field(std::ostringstream& out, const JsonField& field) {
    out << "\""
        << json_escape(field.name)
        << "\":";
    if (field.raw) {
        out << field.value;
    } else {
        out << "\""
            << json_escape(field.value)
            << "\"";
    }
}

} // namespace

void JsonObject::string(std::string name, std::string value) {
    fields_.push_back(JsonField{
        std::move(name),
        std::move(value),
        false,
    });
}

void JsonObject::number(std::string name, std::uint64_t value) {
    fields_.push_back(JsonField{
        std::move(name),
        std::to_string(value),
        true,
    });
}

void JsonObject::boolean(std::string name, bool value) {
    fields_.push_back(JsonField{
        std::move(name),
        value ? "true" : "false",
        true,
    });
}

void JsonObject::raw(std::string name, std::string value) {
    fields_.push_back(JsonField{
        std::move(name),
        std::move(value),
        true,
    });
}

const std::vector<JsonField>& JsonObject::fields() const noexcept {
    return fields_;
}

bool JsonObject::empty() const noexcept {
    return fields_.empty();
}

std::size_t JsonObject::field_count() const noexcept {
    return fields_.size();
}

std::string json_escape(std::string_view value) {
    std::string escaped;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                escaped += hex_escape(ch);
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

std::string render_json_object(const JsonObject& object) {
    std::ostringstream out;
    out << "{";
    for (std::size_t i = 0; i < object.fields().size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        append_field(out, object.fields()[i]);
    }
    out << "}";
    return out.str();
}

std::string render_json_object_pretty(const JsonObject& object, unsigned indent) {
    std::ostringstream out;
    std::string padding(indent, ' ');
    out << "{\n";
    for (std::size_t i = 0; i < object.fields().size(); ++i) {
        out << padding;
        append_field(out, object.fields()[i]);
        if (i + 1 != object.fields().size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "}";
    return out.str();
}

std::string render_json_array(const std::vector<JsonObject>& objects) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < objects.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << render_json_object(objects[i]);
    }
    out << "]";
    return out.str();
}

std::string render_json_string_array(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "\""
            << json_escape(values[i])
            << "\"";
    }
    out << "]";
    return out.str();
}

JsonObject make_status_object(std::string status, std::string message) {
    JsonObject object;
    object.string("status", std::move(status));
    object.string("message", std::move(message));
    return object;
}

std::string render_status_json(std::string status,
                               std::string message,
                               const std::vector<std::string>& notes) {
    auto object = make_status_object(
        std::move(status),
        std::move(message));
    if (!notes.empty()) {
        object.raw(
            "notes",
            render_json_string_array(notes));
    }
    return render_json_object(object);
}

bool is_json_literal(std::string_view value) {
    return value == "true"
        || value == "false"
        || value == "null";
}

JsonObject make_count_object(std::string name, std::uint64_t value) {
    JsonObject object;
    object.string(
        "name",
        std::move(name));
    object.number(
        "value",
        value);
    return object;
}

JsonObject make_error_object(std::string code, std::string message) {
    JsonObject object;
    object.string(
        "status",
        "error");
    object.string(
        "code",
        std::move(code));
    object.string(
        "message",
        std::move(message));
    return object;
}

} // namespace aethon::diagnostics
