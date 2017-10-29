#pragma once

#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/Pipeline.hpp"
#include "../UchCommon/Pool.hpp"

class UclPipl : Pipeline<UclPipl, LinkedChunk<256>> {
public:
    inline UclPipl(SOCKET hSocket) : Pipeline(*this, hSocket) {}

public:
    void OnPacket(Buffer vPakBuf) noexcept;
    void OnPassivelyClose() noexcept;
    void OnForciblyClose() noexcept;
    void OnFinalize() noexcept;

};
