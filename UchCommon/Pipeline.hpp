#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IoGroup.hpp"
#include "SockIo.hpp"
#include "Sync.hpp"

template<
    class tUpper,
    class tChunk = LinkedChunk<4096>,
    class tChunkPool = SysPool<tChunk>
>
class Pipeline {
public:
    using Upper = tUpper;
    using Lower = SockIo<Pipeline>;

    using Chunk = tChunk;
    using ChunkPool = tChunkPool;
    using UpChunk = typename ChunkPool::UniquePtr;
    using Buffer = ByteBuffer<Chunk, ChunkPool>;

public:
    Pipeline(const Pipeline &) = delete;
    Pipeline(Pipeline &&) = delete;

    Pipeline(Upper &vUpper, SOCKET hSocket) :
        x_vUpper(vUpper), x_vLower(*this, hSocket), x_vRecvBuf(x_vPool)
    {}

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

    constexpr ChunkPool &GetChunkPool() const noexcept {
        return x_vPool;
    }

public:
    inline Buffer MakeBuffer() const noexcept {
        return {x_vPool};
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

    void OnRead(DWORD dwError, U32 uDone, Chunk *pChunk) noexcept {
        RAII_LOCK(x_mtx);
        if (dwError) {
            x_vPool.Delete(pChunk);
            X_OnError(static_cast<int>(dwError));
            return;
        }
        x_vRecvBuf.PushChunk(x_vPool.Wrap(pChunk));
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
            auto vPakBuf = x_vRecvBuf.Extract(x_uPakSize);
            x_vUpper.OnPacket(std::move(vPakBuf));
            if (x_vRecvBuf.GetSize() >= sizeof(U16)) {
                x_uPakSize = x_vRecvBuf.Read<U16>();
                assert(x_uPakSize >= 2);
            }
            else
                x_uPakSize = 0;
        }
        X_PostRead();
    }

    inline void OnWrite(DWORD dwError, U32 uDone, Chunk *pChunk) {
        UNREFERENCED_PARAMETER(uDone);
        x_vPool.Delete(pChunk);
        RAII_LOCK(x_mtx);
        if (dwError)
            X_OnError(static_cast<int>(dwError));
    }

    void PostPacket(Buffer &vPakBuf) {
        auto uSize = static_cast<U16>(vPakBuf.GetSize());
        if (uSize != vPakBuf.GetSize())
            throw ExnArgTooLarge {uSize_, 65535};
        auto upChunk = GetChunkPool().MakeUnique();
        upChunk->Write(&uSize, sizeof(U16));
        vPakBuf.PrependChunk(std::move(upChunk));
        RAII_LOCK(x_mtx);
        try {
            while (!vPakBuf.IsEmpty())
                x_vLower.Write(vPakBuf.PopChunk().release());
        }
        catch (ExnSockIo<Chunk> &e) {
            x_vPool.Delete(e.pChunk);
            X_OnError(e.nError);
        }
    }

    inline void PostPacket(Buffer &&vPakBuf) {
        PostPacket(vPakBuf);
    }

private:
    inline void X_OnError(int nError) noexcept {
        UNREFERENCED_PARAMETER(nError);
        x_vUpper.OnForciblyClose();
        x_vLower.Close();
    }

    inline void X_PostRead() noexcept {
        auto pChunk = x_vPool.New();
        try {
            x_vLower.PostRead(pChunk);
        }
        catch (ExnSockIo<Chunk> &e) {
            x_vPool.Delete(pChunk);
            X_OnError(e.nError);
        }
        catch (ExnIllegalState) {
            // active shutdown
            x_vPool.Delete(pChunk);
        }
    }

private:
    mutable ChunkPool x_vPool;

    Upper &x_vUpper;
    Lower x_vLower;

    RecursiveMutex x_mtx;
    bool x_bStopping = false;

    Buffer x_vRecvBuf;
    U16 x_uPakSize = 0;


};
