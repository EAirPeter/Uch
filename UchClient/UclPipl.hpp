#pragma once

#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/Pipeline.hpp"
#include "../UchCommon/Pool.hpp"

class UclPipl : public Pipeline<UclPipl, LinkedChunk<256>> {
public:
    UclPipl();

public:
    void OnPacket(Buffer vPakBuf) noexcept;
    void OnPassivelyClose() noexcept;
    void OnForciblyClose() noexcept;
    void OnFinalize() noexcept;
    void Wait() noexcept;

private:
    Mutex x_mtx;
    ConditionVariable x_cv;
    bool x_bDone = false;

};
