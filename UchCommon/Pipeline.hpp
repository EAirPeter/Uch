#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IoGroup.hpp"
#include "SockIo.hpp"
#include "Sync.hpp"

template<class tUpper>
class Pipeline {
private:
    using Upper = tUpper;
    using Lower = SockIo<Pipeline>;

public:
    Pipeline(const Pipeline &) = delete;
    Pipeline(Pipeline &&) = delete;

    Pipeline(Upper &vUpper, SOCKET hSocket) : x_vUpper(vUpper), x_vLower(*this, hSocket) {}

    inline ~Pipeline() {
        RAII_LOCK(x_mtx);
    }

    Pipeline &operator =(const Pipeline &) = delete;
    Pipeline &operator =(Pipeline &&) = delete;

public:
    constexpr Upper &GetUpper() {
        return x_vUpper;
    }

    constexpr Lower &GetLower() {
        return x_vLower;
    }

public:
    inline void AssignToIoGroup(IoGroup &vIoGroup) {
        RAII_LOCK(x_mtx);
        x_vLower.AssignToIoGroup(vIoGroup);
        X_PostRead();
    }

    inline void Shutdown() noexcept {
        RAII_LOCK(x_mtx);
        x_vLower.Shutdown();
    }

    inline void Close() noexcept {
        RAII_LOCK(x_mtx);
        x_vLower.Close();
    }

    inline void OnFinalize() noexcept {
        RAII_LOCK(x_mtx);
        x_vUpper.OnFinalize();
    }

    void OnRead(DWORD dwError, U32 uDone, ByteChunk *pChunk) noexcept {
        RAII_LOCK(x_mtx);
        if (dwError) {
            x_vRecvBuf.EndRecv(0, pChunk);
            X_OnError(static_cast<int>(dwError));
            return;
        }
        x_vRecvBuf.EndRecv(uDone, pChunk);
        if (!uDone) {
            x_vUpper.OnPassivelyClose();
            x_vLower.Close();
            return;
        }
        if (!x_uPakSize && x_vRecvBuf.GetSize() >= sizeof(U16)) {
            x_uPakSize = x_vRecvBuf.Read<U16>();
            assert(x_uPakSize >= 2);
        }
        while (x_uPakSize && x_vRecvBuf.GetSize() >= x_uPakSize) {
            auto uPakId = x_vRecvBuf.Read<U16>();
            auto vPakBuf = x_vRecvBuf.Extract(x_uPakSize - sizeof(U16));
            x_vUpper.OnPacket(uPakId, std::move(vPakBuf));
            if (x_vRecvBuf.GetSize() >= sizeof(U16)) {
                x_uPakSize = x_vRecvBuf.Read<U16>();
                assert(x_uPakSize >= 2);
            }
            else
                x_uPakSize = 0;
        }
        X_PostRead();
    }

    inline void OnWrite(DWORD dwError, U32 uDone, std::unique_ptr<ByteChunk> upChunk) {
        UNREFERENCED_PARAMETER(uDone);
        UNREFERENCED_PARAMETER(upChunk);
        RAII_LOCK(x_mtx);
        if (dwError)
            X_OnError(static_cast<int>(dwError));
    }

    void PostPacket(U16 uPakId, ByteBuffer &vPakBuf) {
        vPakBuf.FormPacket(uPakId);
        RAII_LOCK(x_mtx);
        try {
            while (!vPakBuf.IsEmpty())
                x_vLower.Write(vPakBuf.PopChunk());
        }
        catch (ExnSockWrite<ByteChunk> &e) {
            X_OnError(e.nError);
        }
    }

    inline void PostPacket(U16 uPakId, ByteBuffer &&vPakBuf) {
        PostPacket(uPakId, vPakBuf);
    }

private:
    inline void X_OnError(int nError) noexcept {
        UNREFERENCED_PARAMETER(nError);
        x_vUpper.OnForciblyClose();
        x_vLower.Close();
    }

    inline void X_PostRead() noexcept {
        auto pChunk = x_vRecvBuf.BeginRecv(x_uPakSize);
        try {
            x_vLower.PostRead(pChunk);
        }
        catch (ExnSockRead<ByteChunk> &e) {
            x_vRecvBuf.EndRecv(0, pChunk);
            X_OnError(e.nError);
        }
        catch (ExnIllegalState) {
            // active shutdown
            x_vRecvBuf.EndRecv(0, pChunk);
        }
    }

private:
    Upper &x_vUpper;
    Lower x_vLower;

    RecursiveMutex x_mtx;
    bool x_bStopping = false;

    ByteBuffer x_vRecvBuf;
    U16 x_uPakSize = 0;

};
