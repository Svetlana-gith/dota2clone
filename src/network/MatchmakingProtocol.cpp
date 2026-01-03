#include "MatchmakingProtocol.h"
#include <cstring>

namespace WorldEditor::Matchmaking::Wire {

void CopyCString(char* dst, size_t dstSize, const std::string& src) {
    if (!dst || dstSize == 0) return;
    std::memset(dst, 0, dstSize);
    const size_t n = std::min(dstSize - 1, src.size());
    if (n > 0) {
        std::memcpy(dst, src.data(), n);
    }
}

bool BuildPacket(std::vector<u8>& out,
                 MatchmakingMessageType type,
                 u64 playerId,
                 u64 lobbyId,
                 const void* payload,
                 u32 payloadSize) {
    // payload can be null when payloadSize=0
    if (payloadSize > 0 && payload == nullptr) return false;

    MMHeader h;
    h.magic = kMagic;
    h.version = kVersion;
    h.type = static_cast<u16>(type);
    h.payloadSize = payloadSize;
    h.playerId = playerId;
    h.lobbyId = lobbyId;

    out.resize(sizeof(MMHeader) + payloadSize);
    std::memcpy(out.data(), &h, sizeof(MMHeader));
    if (payloadSize > 0) {
        std::memcpy(out.data() + sizeof(MMHeader), payload, payloadSize);
    }
    return true;
}

bool ParsePacket(const void* data,
                 size_t size,
                 MMHeader& outHeader,
                 const void*& outPayload,
                 u32& outPayloadSize) {
    outPayload = nullptr;
    outPayloadSize = 0;

    if (!data || size < sizeof(MMHeader)) return false;

    MMHeader h;
    std::memcpy(&h, data, sizeof(MMHeader));

    if (h.magic != kMagic) return false;
    if (h.version != kVersion) return false;

    const size_t total = sizeof(MMHeader) + static_cast<size_t>(h.payloadSize);
    if (size < total) return false;

    outHeader = h;
    outPayloadSize = h.payloadSize;
    outPayload = (h.payloadSize > 0) ? (static_cast<const u8*>(data) + sizeof(MMHeader)) : nullptr;
    return true;
}

} // namespace WorldEditor::Matchmaking::Wire

