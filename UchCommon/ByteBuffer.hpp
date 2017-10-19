#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "IntrList.hpp"

class ByteBuffer {
private:
    constexpr static U32 kuChunkSize = 16;

public:
    ByteBuffer() noexcept = default;

    inline ByteBuffer(const ByteBuffer &vBuf) {
        Append(vBuf);
    }

    inline ByteBuffer(ByteBuffer &&vBuf) noexcept {
        vBuf.Swap(*this);
    }

    inline ByteBuffer(std::unique_ptr<ByteChunk> upChunk) noexcept {
        PushChunk(std::move(upChunk));
    }

    inline ByteBuffer(const void *pData, U32 uSize) {
        WriteBytes(pData, uSize);
    }

    inline ByteBuffer &operator =(const ByteBuffer &vBuf) {
        ByteBuffer(vBuf).Swap(*this);
        return *this;
    }

    inline ByteBuffer &operator =(ByteBuffer &&vBuf) noexcept {
        vBuf.Swap(*this);
        return *this;
    }

    inline void Swap(ByteBuffer &vBuf) noexcept {
        using std::swap;
        swap(x_uSize, vBuf.x_uSize);
        swap(x_liChunks, vBuf.x_liChunks);
    }

    friend inline void swap(ByteBuffer &vLhs, ByteBuffer &vRhs) noexcept {
        vRhs.Swap(vLhs);
    }

public:
    constexpr bool IsEmpty() const noexcept {
        return !x_uSize;
    }

    constexpr U32 GetSize() const noexcept {
        return x_uSize;
    }

    constexpr void Clear() noexcept {
        x_uSize = 0;
        x_liChunks.Clear();
    }

private:
    inline ByteChunk *X_Reserve(U32 uSize) noexcept;
    inline void X_DiscardUnsafe(U32 uSize) noexcept;
    inline void X_PeekUnsafe(void *pData, U32 uSize) const noexcept;
    inline void X_ReadUnsafe(void *pData, U32 uSize) noexcept;
    inline void X_WriteUnsafe(ByteChunk *&pChunk, const void *pData, U32 uSize) noexcept;
    inline ByteBuffer X_ExtractUnsafe(U32 uSize) noexcept;

public:
    void Discard(U32 uSize);
    void PeekBytes(void *pData, U32 uSize) const;
    void ReadBytes(void *pData, U32 uSize);
    void WriteBytes(const void *pData, U32 uSize) noexcept;
#ifdef BYTEBUFFER_NEED_AS_MUCH
    U32 DiscardAsMuch(U32 uSize) noexcept;
    U32 PeekAsMuch(void *pData, U32 uSize) const noexcept;
    U32 ReadAsMuch(void *pData, U32 uSize) noexcept;
    U32 WriteAsMuch(const void *pData, U32 uSize) noexcept;
#endif

public:
    inline void Splice(ByteBuffer &&vBuf) noexcept {
        Splice(vBuf);
    }

    void Splice(ByteBuffer &vBuf) noexcept;
    void Append(const ByteBuffer &vBuf) noexcept;
    ByteBuffer Extract(U32 uSize);
#ifdef BYTEBUFFER_NEED_AS_MUCH
    ByteBuffer ExtractAsMuch(U32 uSize) noexcept;
#endif

public:
    template<class tPOD>
    inline tPOD Peek() {
        static_assert(std::is_pod_v<tPOD>, "POD type is required");
        tPOD vRes;
        PeekBytes(&vRes, sizeof(tPOD));
        return std::move(vRes);
    }

    template<class tPOD>
    inline tPOD Read() {
        static_assert(std::is_pod_v<tPOD>, "POD type is required");
        tPOD vRes;
        ReadBytes(&vRes, sizeof(tPOD));
        return std::move(vRes);
    }

    template<class tPOD>
    inline void Write(const tPOD &vData) noexcept {
        static_assert(std::is_pod_v<tPOD>, "POD type is required");
        WriteBytes(&vData, sizeof(tPOD));
    }

    String ReadUtf8();
    void WriteUtf8(const String &sData);

public:
    inline void FormPacket(U16 uPakId) {
        auto uSize_ = x_uSize + sizeof(U16);
        auto uSize = static_cast<U16>(uSize_);
        if (uSize != uSize_)
            throw ExnArgTooLarge {uSize_, 65535};
        auto uSzHdr = static_cast<U32>(sizeof(U16) + sizeof(U16));
        x_uSize += uSzHdr;
        auto pHead = x_liChunks.GetHead();
        if (pHead == x_liChunks.GetNil() || pHead->GetPrependable() < uSzHdr) {
            auto upHead = X_MakeChunk(uSzHdr);
            upHead->ToEnd();
            upHead->Prepend(&uPakId, sizeof(U16));
            upHead->Prepend(&uSize, sizeof(U16));
            x_liChunks.PushHead(std::move(upHead));
        }
        else {
            pHead->Prepend(&uPakId, sizeof(U16));
            pHead->Prepend(&uSize, sizeof(U16));
        }
    }

    inline ByteChunk *BeginRecv(U32 uSize) noexcept {
        auto pTail = x_liChunks.GetTail();
        if (pTail == x_liChunks.GetNil() || !pTail->IsWritable())
            return X_MakeChunk(uSize).release();
        return pTail;
    }

    inline void EndRecv(U32 uSize, ByteChunk *pChunk) noexcept {
        if (uSize) {
            x_uSize += uSize;
            if (pChunk != x_liChunks.GetTail())
                x_liChunks.PushTail(std::unique_ptr<ByteChunk>(pChunk));
        }
        else if (pChunk != x_liChunks.GetTail())
            delete pChunk;
    }

    inline void PushChunk(std::unique_ptr<ByteChunk> upChunk) noexcept {
        x_uSize += upChunk->GetReadable();
        x_liChunks.PushTail(std::move(upChunk));
    }

    inline std::unique_ptr<ByteChunk> PopChunk() {
        if (IsEmpty())
            throw ExnNoEnoughData();
        auto upChunk = x_liChunks.PopHead();
        x_uSize -= upChunk->GetReadable();
        return std::move(upChunk);
    }

private:
    constexpr ByteBuffer(U32 uSize, IntrList<ByteChunk> &&vList) : x_uSize(uSize), x_liChunks(std::move(vList)) {}

    static inline std::unique_ptr<ByteChunk> X_MakeChunk(U32 uSize) {
        return ByteChunk::MakeUnique(uSize | kuChunkSize);
    }

private:
    U32 x_uSize = 0;
    IntrList<ByteChunk> x_liChunks;

};