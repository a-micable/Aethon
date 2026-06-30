#include "aethon/protocol/extension_registry.hpp"

#include "aethon/common/bytes.hpp"
#include "aethon/protocol/tlv.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::protocol {
namespace {

std::size_t count_type(const std::vector<ExtensionHeader>& extensions, std::uint16_t type) {
    return static_cast<std::size_t>(std::count_if(
        extensions.begin(),
        extensions.end(),
        [type](const ExtensionHeader& extension) {
            return extension.type == type;
        }));
}

std::string render_unsigned_value(const ExtensionHeader& extension) {
    TlvField field;
    field.type = extension.type;
    field.value = extension.value;
    auto value = tlv_unsigned(field);
    if (!value) {
        return "invalid_unsigned";
    }
    return std::to_string(*value);
}

std::string render_string_value(const ExtensionHeader& extension) {
    TlvField field;
    field.type = extension.type;
    field.value = extension.value;
    auto value = tlv_string(field);
    if (!value) {
        return "invalid_string";
    }
    return *value;
}

std::string render_tlv_value(const ExtensionHeader& extension) {
    try {
        auto fields = parse_tlv_fields(extension.value, {32, 4096, false});
        std::ostringstream out;
        out << fields.size() << " tlv fields";
        return out.str();
    } catch (...) {
        return "invalid_tlv";
    }
}

} // namespace

void ExtensionRegistry::add(ExtensionDescriptor descriptor) {
    auto existing = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const ExtensionDescriptor& item) {
            return item.type == descriptor.type;
        });
    if (existing == descriptors_.end()) {
        descriptors_.push_back(std::move(descriptor));
    } else {
        *existing = std::move(descriptor);
    }
}

std::optional<ExtensionDescriptor> ExtensionRegistry::find(std::uint16_t type) const {
    auto it = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [type](const ExtensionDescriptor& descriptor) {
            return descriptor.type == type;
        });
    if (it == descriptors_.end()) {
        return std::nullopt;
    }
    return *it;
}

const std::vector<ExtensionDescriptor>& ExtensionRegistry::descriptors() const noexcept {
    return descriptors_;
}

ExtensionRegistry default_extension_registry() {
    ExtensionRegistry registry;
    registry.add({1, "collector_id", ExtensionValueKind::unsigned_integer, 1, 8, false});
    registry.add({2, "site_name", ExtensionValueKind::utf8_string, 1, 128, false});
    registry.add({3, "capture_tags", ExtensionValueKind::nested_tlv, 0, 4096, true});
    registry.add({4, "firmware_build", ExtensionValueKind::utf8_string, 1, 128, false});
    registry.add({5, "operator_note", ExtensionValueKind::utf8_string, 0, 512, true});
    return registry;
}

std::vector<ExtensionValidationIssue> validate_extensions(
    const std::vector<ExtensionHeader>& extensions,
    const ExtensionRegistry& registry) {
    std::vector<ExtensionValidationIssue> issues;
    for (const auto& extension : extensions) {
        auto descriptor = registry.find(extension.type);
        if (!descriptor) {
            issues.push_back({extension.type, "unknown extension type"});
            continue;
        }
        if (extension.value.size() < descriptor->min_size) {
            issues.push_back({extension.type, "extension value is shorter than allowed"});
        }
        if (extension.value.size() > descriptor->max_size) {
            issues.push_back({extension.type, "extension value is larger than allowed"});
        }
        if (!descriptor->repeatable && count_type(extensions, extension.type) > 1) {
            issues.push_back({extension.type, "extension type is not repeatable"});
        }
    }
    return issues;
}

std::string extension_value_kind_name(ExtensionValueKind kind) {
    switch (kind) {
    case ExtensionValueKind::opaque:
        return "opaque";
    case ExtensionValueKind::unsigned_integer:
        return "unsigned_integer";
    case ExtensionValueKind::utf8_string:
        return "utf8_string";
    case ExtensionValueKind::nested_tlv:
        return "nested_tlv";
    }
    return "unknown";
}

std::string render_extension(const ExtensionHeader& extension,
                             const ExtensionRegistry& registry) {
    auto descriptor = registry.find(extension.type);
    std::ostringstream out;
    out << "extension type="
        << extension.type
        << " bytes="
        << extension.value.size();
    if (!descriptor) {
        out << " name=unknown value="
            << to_hex(extension.value);
        return out.str();
    }

    out << " name="
        << descriptor->name
        << " kind="
        << extension_value_kind_name(descriptor->value_kind)
        << " value=";
    switch (descriptor->value_kind) {
    case ExtensionValueKind::opaque:
        out << to_hex(extension.value);
        break;
    case ExtensionValueKind::unsigned_integer:
        out << render_unsigned_value(extension);
        break;
    case ExtensionValueKind::utf8_string:
        out << render_string_value(extension);
        break;
    case ExtensionValueKind::nested_tlv:
        out << render_tlv_value(extension);
        break;
    }
    return out.str();
}

} // namespace aethon::protocol
