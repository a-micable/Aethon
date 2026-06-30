#pragma once

#include "aethon/protocol/types.hpp"

#include <string>
#include <vector>

namespace aethon::protocol {

enum class ProtocolFeature {
    routing,
    fragments,
    capabilities,
    extensions,
    encryption,
};

struct VersionFeature {
    ProtocolVersion version = ProtocolVersion::v1;
    ProtocolFeature feature = ProtocolFeature::routing;
    bool supported = false;
};

[[nodiscard]] bool feature_supported(ProtocolVersion version, ProtocolFeature feature);
[[nodiscard]] std::vector<VersionFeature> build_version_matrix();
[[nodiscard]] std::string protocol_feature_name(ProtocolFeature feature);
[[nodiscard]] std::string render_version_matrix();

} // namespace aethon::protocol
