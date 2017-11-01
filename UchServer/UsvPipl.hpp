#pragma once

#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/Pipeline.hpp"

class UsvPipl : public Pipeline<UsvPipl, LinkedChunk<256>> {
public:
    UsvPipl(SOCKET hSocket);

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
