#pragma once

#include "Common.hpp"

#include "IntrList.hpp"

template<
    class tChunk,
    class tChunkPool = SysPool<tChunk>,
    U32 kuChunkSize = tChunk::GetCapacity()
>
class ByteBuffer : protected IntrList<tChunk, tChunkPool> {
public:
    using Chunk = tChunk;
    using ChunkPool = tChunkPool;
    using UpChunk = typename ChunkPool::UniquePtr;

public:
    inline ByteBuffer(ChunkPool &vPool) noexcept : IntrList(vPool) {}

    inline ByteBuffer(const ByteBuffer &vBuf) : IntrList(vBuf.GetElemPool()) {
        Append(vBuf);
    }

    inline ByteBuffer(ByteBuffer &&vBuf) noexcept : IntrList(std::move(vBuf)) {
        using std::swap;
        swap(x_uSize, vBuf.x_uSize);
    }

    inline ByteBuffer(UpChunk upChunk) noexcept : IntrList(upChunk->GetPool()) {
        PushChunk(std::move(upChunk));
    }

    inline ByteBuffer(const void *pData, U32 uSize, ChunkPool &vPool) : IntrList(vPool) {
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
        IntrList::Swap(vBuf);
        using std::swap;
        swap(x_uSize, vBuf.x_uSize);
    }

    friend inline void swap(ByteBuffer &vLhs, ByteBuffer &vRhs) noexcept {
        vRhs.Swap(vLhs);
    }

public:
    constexpr ChunkPool &GetChunkPool() const noexcept {
        return const_cast<ChunkPool &>(IntrList::GetElemPool());
    }

    constexpr bool IsEmpty() const noexcept {
        return !x_uSize;
    }

    constexpr U32 GetSize() const noexcept {
        return x_uSize;
    }

    constexpr void Clear() noexcept {
        x_uSize = 0;
        IntrList::Clear();
    }

private:
    // uSize > 0
    inline Chunk *X_Reserve(U32 uSize) noexcept {
        if (IntrList::IsEmpty())
            IntrList::PushTail(GetChunkPool().MakeUnique());
        auto pChunk = IntrList::GetTail();
        if (pChunk->GetWritable() >= uSize)
            return pChunk;
        uSize -= pChunk->GetWritable();
        while (uSize > kuChunkSize) {
            IntrList::PushTail(GetChunkPool().MakeUnique());
            uSize -= kuChunkSize;
        }
        IntrList::PushTail(GetChunkPool().MakeUnique());
        return pChunk;
    }

    // uSize > 0, uSize <= x_uSize
    inline void X_DiscardUnsafe(U32 uSize) noexcept {
        x_uSize -= uSize;
        while (uSize) {
            auto pChunk = IntrList::GetHead();
            auto uToDiscard = std::min(pChunk->GetReadable(), uSize);
            pChunk->IncReader(uToDiscard);
            uSize -= uToDiscard;
            if (!pChunk->IsReadable())
                IntrList::Remove(pChunk);
        }
    }

    // uSize > 0, uSize <= x_uSize
    inline void X_PeekUnsafe(void *pData, U32 uSize) const noexcept {
        auto pDst = reinterpret_cast<Byte *>(pData);
        auto pChunk = IntrList::GetHead();
        while (uSize) {
            auto uToPeek = std::min(pChunk->GetReadable(), uSize);
            pChunk->Peek(pDst, uToPeek);
            pDst += uToPeek;
            uSize -= uToPeek;
            if (uToPeek == pChunk->GetReadable())
                pChunk = pChunk->GetNext();
        }
    }

    // uSize > 0, uSize <= x_uSize
    inline void X_ReadUnsafe(void *pData, U32 uSize) noexcept {
        x_uSize -= uSize;
        auto pDst = reinterpret_cast<Byte *>(pData);
        while (uSize) {
            auto pChunk = IntrList::GetHead();
            auto uToRead = std::min(pChunk->GetReadable(), uSize);
            pChunk->Read(pDst, uToRead);
            pDst += uToRead;
            uSize -= uToRead;
            if (!pChunk->IsReadable())
                IntrList::Remove(pChunk);
        }
    }

    // uSize > 0, X_Reserve() invoked
    inline void X_WriteUnsafe(Chunk *&pChunk, const void *pData, U32 uSize) noexcept {
        x_uSize += uSize;
        auto pSrc = reinterpret_cast<const Byte *>(pData);
        while (uSize) {
            auto uToWrite = std::min(pChunk->GetWritable(), uSize);
            pChunk->Write(pSrc, uToWrite);
            pSrc += uToWrite;
            uSize -= uToWrite;
            if (!pChunk->IsWritable())
                pChunk = pChunk->GetNext();
        }
    }

    // uSize > 0, uSize <= x_uSize
    inline ByteBuffer X_ExtractUnsafe(U32 uSize) noexcept {
        x_uSize -= uSize;
        auto pTail = IntrList::GetHead();
        U32 uAcc = pTail->GetReadable();
        while (uAcc < uSize) {
            pTail = pTail->GetNext();
            uAcc += pTail->GetReadable();
        }
        auto vList = IntrList::ExtractFromHeadTo(pTail);
        if (uAcc != uSize) {
            auto uToAdjust = uAcc - uSize;
            IntrList::PushHead(GetChunkPool().MakeUnique());
            auto pHead = IntrList::GetHead();
            pHead->Write(pTail->GetWriter() - uToAdjust, uToAdjust);
            pTail->DecWriter(uToAdjust);
        }
        return {uSize, std::move(vList)};
    }

public:
    inline void IncReader(U32 uSize) {
        if (!uSize)
            return;
        if (x_uSize < uSize)
            throw ExnNoEnoughData {uSize, x_uSize};
        X_DiscardUnsafe(uSize);
    }

    inline void PeekBytes(void *pData, U32 uSize) const {
        if (!uSize)
            return;
        if (x_uSize < uSize)
            throw ExnNoEnoughData {uSize, x_uSize};
        X_PeekUnsafe(pData, uSize);
    }

    inline void ReadBytes(void *pData, U32 uSize) {
        if (!uSize)
            return;
        if (x_uSize < uSize)
            throw ExnNoEnoughData {uSize, x_uSize};
        X_ReadUnsafe(pData, uSize);
    }

    inline void WriteBytes(const void *pData, U32 uSize) noexcept {
        if (!uSize)
            return;
        auto pChunk = X_Reserve(uSize);
        X_WriteUnsafe(pChunk, pData, uSize);
    }

#ifdef BYTEBUFFER_NEED_AS_MUCH
    inline U32 DiscardAsMuch(U32 uSize) noexcept {
        uSize = std::min(uSize, x_uSize);
        X_DiscardUnsafe(uSize);
        return uSize;
    }

    inline U32 PeekAsMuch(void *pData, U32 uSize) const noexcept {
        uSize = std::min(uSize, x_uSize);
        X_PeekUnsafe(pData, uSize);
        return uSize;
    }

    inline U32 ReadAsMuch(void *pData, U32 uSize) noexcept {
        uSize = std::min(uSize, x_uSize);
        X_ReadUnsafe(pData, uSize);
        return uSize;
    }

    inline U32 WriteAsMuch(const void *pData, U32 uSize) noexcept {
        X_Reserve(uSize);
        X_WriteUnsafe(pData, uSize);
        return uSize;
    }

#endif

public:
    void Splice(ByteBuffer &vBuf) noexcept {
        x_uSize += std::exchange(vBuf.x_uSize, 0);
        IntrList::Splice(vBuf);
    }

    inline void Splice(ByteBuffer &&vBuf) noexcept {
        Splice(vBuf);
    }

    void Append(const ByteBuffer &vBuf) noexcept {
        if (vBuf.IsEmpty())
            return;
        auto pChunk = X_Reserve(vBuf.GetSize());
        for (auto pOther = vBuf.IntrList::GetHead(); pOther != vBuf.IntrList::GetNil(); pOther = pOther->GetNext())
            X_WriteUnsafe(pChunk, pOther->GetReader(), pOther->GetReadable());
    }

    ByteBuffer Extract(U32 uSize) {
        if (!uSize)
            return {GetChunkPool()};
        if (x_uSize < uSize)
            throw ExnNoEnoughData {uSize, x_uSize};
        return X_ExtractUnsafe(uSize);
    }

#ifdef BYTEBUFFER_NEED_AS_MUCH
    ByteBuffer ExtractAsMuch(U32 uSize) noexcept {
        uSize = std::min(uSize, x_uSize);
        return X_ExtractUnsafe(uSize);
    }

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

    inline String ReadUtf8() {
        auto uLen = static_cast<U32>(Peek<U16>());
        if (!uLen) {
            X_DiscardUnsafe(sizeof(U16));
            return {};
        }
        if (x_uSize < uLen + sizeof(U16))
            throw ExnNoEnoughData {uLen + sizeof(U16), x_uSize};
        X_DiscardUnsafe(sizeof(U16));
        X_ReadUnsafe(g_szUtf8Buf, uLen);
        uLen = ConvertUtf8ToWide(static_cast<int>(uLen));
        return {g_szWideBuf, uLen};
    }

    inline void WriteUtf8(const String &sData) {
        auto uLen = ConvertWideToUtf8(sData);
        auto pChunk = X_Reserve(sizeof(U16) + uLen);
        X_WriteUnsafe(pChunk, &uLen, sizeof(U16));
        if (uLen)
            X_WriteUnsafe(pChunk, g_szUtf8Buf, uLen);
    }

public:
    inline void PushChunk(UpChunk upChunk) noexcept {
        if (upChunk->IsReadable()) {
            x_uSize += upChunk->GetReadable();
            IntrList::PushTail(std::move(upChunk));
        }
    }

    inline UpChunk PopChunk() {
        if (IntrList::IsEmpty())
            throw ExnNoEnoughData();
        auto upChunk = IntrList::PopHead();
        x_uSize -= upChunk->GetReadable();
        return std::move(upChunk);
    }

    inline void PrependChunk(UpChunk upChunk) noexcept {
        if (upChunk->IsReadable()) {
            x_uSize += upChunk->GetReadable();
            IntrList::PushHead(std::move(upChunk));
        }
    }

    inline UpChunk DropChunk() {
        if (IntrList::IsEmpty())
            throw ExnNoEnoughData();
        auto upChunk = IntrList::PopTail();
        x_uSize -= upChunk->GetReadable();
        return std::move(upChunk);
    }

private:
    constexpr ByteBuffer(U32 uSize, IntrList &&vList) : IntrList(std::move(vList)), x_uSize(uSize) {}

private:
    U32 x_uSize = 0;

};
