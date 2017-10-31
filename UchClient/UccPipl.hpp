#pragma once

#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/Pipeline.hpp"
#include "../UchCommon/Pool.hpp"

class UccPipl : public Pipeline<UccPipl, LinkedChunk<256>> {
public:
    UccPipl(SOCKET hSocket);
    UccPipl(SOCKET hSocket, const String &sUser);

    constexpr const String &GetUser() const noexcept {
        return x_sUser;
    }

public:
    void OnPacket(Buffer vPakBuf) noexcept;
    void OnPassivelyClose() noexcept;
    void OnForciblyClose() noexcept;
    void OnFinalize() noexcept;

private:
    String x_sUser;

};
