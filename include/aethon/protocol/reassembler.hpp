#pragma once

#include "aethon/protocol/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace aethon::protocol {

struct ReassemblyKey {
    DeviceId device = 0;
    std::uint32_t stream_id = 0;

    friend bool operator<(const ReassemblyKey& lhs, const ReassemblyKey& rhs) noexcept {
        if (lhs.device != rhs.device) {
            return lhs.device < rhs.device;
        }
        return lhs.stream_id < rhs.stream_id;
    }
};

struct ReassemblyStats {
    std::uint64_t fragments_seen = 0;
    std::uint64_t completed_messages = 0;
    std::uint64_t duplicate_fragments = 0;
    std::uint64_t rejected_fragments = 0;
    std::size_t pending_streams = 0;
};

class FragmentReassembler {
public:
    explicit FragmentReassembler(std::size_t max_payload_bytes = 1024 * 1024);

    [[nodiscard]] std::optional<Packet> push(Packet fragment);
    void clear(DeviceId device, std::uint32_t stream_id);
    void clear_all();

    [[nodiscard]] ReassemblyStats stats() const;

private:
    struct PendingStream {
        Packet first_packet;
        std::vector<Bytes> fragments;
        std::vector<bool> present;
        std::size_t received = 0;
        std::size_t total_payload = 0;
    };

    std::size_t max_payload_bytes_;
    std::map<ReassemblyKey, PendingStream> pending_;
    ReassemblyStats stats_;
};

} // namespace aethon::protocol
