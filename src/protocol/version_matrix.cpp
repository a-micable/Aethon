#include "aethon/protocol/version_matrix.hpp"

#include "aethon/protocol/inspector.hpp"

#include <array>
#include <sstream>

namespace aethon::protocol {

bool feature_supported(ProtocolVersion version, ProtocolFeature feature) {
    switch (feature) {
    case ProtocolFeature::routing:
        return version >= ProtocolVersion::v2;
    case ProtocolFeature::fragments:
        return version >= ProtocolVersion::v2;
    case ProtocolFeature::capabilities:
        return version >= ProtocolVersion::v3;
    case ProtocolFeature::extensions:
        return version >= ProtocolVersion::v3;
    case ProtocolFeature::encryption:
        return version >= ProtocolVersion::v3;
    }
    return false;
}

std::vector<VersionFeature> build_version_matrix() {
    constexpr std::array versions{
        ProtocolVersion::v1,
        ProtocolVersion::v2,
        ProtocolVersion::v3,
    };
    constexpr std::array features{
        ProtocolFeature::routing,
        ProtocolFeature::fragments,
        ProtocolFeature::capabilities,
        ProtocolFeature::extensions,
        ProtocolFeature::encryption,
    };
    std::vector<VersionFeature> matrix;
    for (auto version : versions) {
        for (auto feature : features) {
            matrix.push_back(VersionFeature{
                version,
                feature,
                feature_supported(version, feature),
            });
        }
    }
    return matrix;
}

std::string protocol_feature_name(ProtocolFeature feature) {
    switch (feature) {
    case ProtocolFeature::routing:
        return "routing";
    case ProtocolFeature::fragments:
        return "fragments";
    case ProtocolFeature::capabilities:
        return "capabilities";
    case ProtocolFeature::extensions:
        return "extensions";
    case ProtocolFeature::encryption:
        return "encryption";
    }
    return "unknown";
}

std::string render_version_matrix() {
    std::ostringstream out;
    out << "version_matrix\n";
    for (const auto& item : build_version_matrix()) {
        out << "  "
            << version_name(item.version)
            << " "
            << protocol_feature_name(item.feature)
            << "="
            << (item.supported ? "yes" : "no")
            << "\n";
    }
    return out.str();
}

} // namespace aethon::protocol
