#pragma once

#include "aethon/protocol/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::protocol {

enum class ExtensionValueKind {
    opaque,
    unsigned_integer,
    utf8_string,
    nested_tlv,
};

struct ExtensionDescriptor {
    std::uint16_t type = 0;
    std::string name;
    ExtensionValueKind value_kind = ExtensionValueKind::opaque;
    std::size_t min_size = 0;
    std::size_t max_size = 65535;
    bool repeatable = true;
};

struct ExtensionValidationIssue {
    std::uint16_t type = 0;
    std::string message;
};

class ExtensionRegistry {
public:
    void add(ExtensionDescriptor descriptor);
    [[nodiscard]] std::optional<ExtensionDescriptor> find(std::uint16_t type) const;
    [[nodiscard]] const std::vector<ExtensionDescriptor>& descriptors() const noexcept;

private:
    std::vector<ExtensionDescriptor> descriptors_;
};

[[nodiscard]] ExtensionRegistry default_extension_registry();
[[nodiscard]] std::vector<ExtensionValidationIssue> validate_extensions(
    const std::vector<ExtensionHeader>& extensions,
    const ExtensionRegistry& registry);
[[nodiscard]] std::string extension_value_kind_name(ExtensionValueKind kind);
[[nodiscard]] std::string render_extension(const ExtensionHeader& extension,
                                           const ExtensionRegistry& registry);

} // namespace aethon::protocol
