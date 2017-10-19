#include "Common.hpp"

#include "ByteBuffer.hpp"

// uSize > 0
inline ByteChunk *ByteBuffer::X_Reserve(U32 uSize) noexcept {
    auto pChunk = x_liChunks.GetTail();
    if (pChunk == x_liChunks.GetNil() || !pChunk->IsWritable()) {
        x_liChunks.PushTail(X_MakeChunk(uSize));
        return x_liChunks.GetTail();
    }
    auto uTailWritable = pChunk->GetWritable();
    if (uTailWritable >= uSize)
        return pChunk;
    uSize -= uTailWritable;
    x_liChunks.PushTail(X_MakeChunk(uSize));
    return pChunk;
}

// uSize > 0, uSize <= x_uSize
inline void ByteBuffer::X_DiscardUnsafe(U32 uSize) noexcept {
    x_uSize -= uSize;
    for (;;) {
        auto pChunk = x_liChunks.GetHead();
        auto uToDiscard = std::min(pChunk->GetReadable(), uSize);
        pChunk->Discard(uToDiscard);
        uSize -= uToDiscard;
        if (!uSize)
            break;
        if (!pChunk->IsReadable())
            pChunk->Remove();
    }
    if (!x_uSize)
        x_liChunks.GetTail()->ToBegin();
    else if (!x_liChunks.GetHead()->IsReadable())
        x_liChunks.PopHead();
}

// uSize > 0, uSize <= x_uSize
inline void ByteBuffer::X_PeekUnsafe(void *pData, U32 uSize) const noexcept {
    auto pDst = reinterpret_cast<Byte *>(pData);
    auto pChunk = x_liChunks.GetHead();
    for (;;) {
        auto uToPeek = std::min(pChunk->GetReadable(), uSize);
        pChunk->Peek(pDst, uToPeek);
        pDst += uToPeek;
        uSize -= uToPeek;
        if (!uSize)
            break;
        if (uToPeek == pChunk->GetReadable())
            pChunk = pChunk->GetNext();
    }
}

// uSize > 0, uSize <= x_uSize
inline void ByteBuffer::X_ReadUnsafe(void *pData, U32 uSize) noexcept {
    x_uSize -= uSize;
    auto pDst = reinterpret_cast<Byte *>(pData);
    for (;;) {
        auto pChunk = x_liChunks.GetHead();
        auto uToRead = std::min(pChunk->GetReadable(), uSize);
        pChunk->Read(pDst, uToRead);
        pDst += uToRead;
        uSize -= uToRead;
        if (!uSize)
            break;
        if (!pChunk->IsReadable())
            pChunk->Remove();
    }
    if (!x_uSize)
        x_liChunks.GetTail()->ToBegin();
    else if (!x_liChunks.GetHead()->IsReadable())
        x_liChunks.PopHead();
}

// uSize > 0, X_Reserve() invoked
inline void ByteBuffer::X_WriteUnsafe(ByteChunk *&pChunk, const void *pData, U32 uSize) noexcept {
    x_uSize += uSize;
    auto pSrc = reinterpret_cast<const Byte *>(pData);
    for (;;) {
        auto uToWrite = std::min(pChunk->GetWritable(), uSize);
        pChunk->Write(pSrc, uToWrite);
        pSrc += uToWrite;
        uSize -= uToWrite;
        if (!pChunk->IsWritable())
            pChunk = pChunk->GetNext();
        if (!uSize)
            break;
    }
}

// uSize > 0, uSize <= x_uSize
inline ByteBuffer ByteBuffer::X_ExtractUnsafe(U32 uSize) noexcept {
    x_uSize -= uSize;
    auto pTail = x_liChunks.GetHead();
    U32 uAcc = 0;
    for (;;) {
        uAcc += pTail->GetReadable();
        if (uAcc >= uSize)
            break;
        pTail = pTail->GetNext();
    }
    auto vList = x_liChunks.ExtractFromHeadTo(pTail);
    if (uAcc != uSize) {
        auto uToAdjust = uAcc - uSize;
        auto pHead = x_liChunks.GetHead();
        if (pHead != x_liChunks.GetNil() && pHead->IsPrependable()) {
            auto uToSuck = std::min(pHead->GetPrependable(), uToAdjust);
            pHead->Suck(*pTail, uToSuck);
            uToAdjust -= uToSuck;
        }
        if (uToAdjust) {
            auto ipChunk = X_MakeChunk(uToAdjust);
            ipChunk->ToEnd();
            ipChunk->Suck(*pTail, uToAdjust);
            x_liChunks.PushHead(std::move(ipChunk));
        }
    }
    return {uSize, std::move(vList)};
}

void ByteBuffer::Discard(U32 uSize) {
    if (!uSize)
        return;
    if (x_uSize < uSize)
        throw ExnNoEnoughData {uSize, x_uSize};
    X_DiscardUnsafe(uSize);
}

void ByteBuffer::PeekBytes(void *pData, U32 uSize) const {
    if (!uSize)
        return;
    if (x_uSize < uSize)
        throw ExnNoEnoughData {uSize, x_uSize};
    X_PeekUnsafe(pData, uSize);
}

void ByteBuffer::ReadBytes(void *pData, U32 uSize) {
    if (!uSize)
        return;
    if (x_uSize < uSize)
        throw ExnNoEnoughData {uSize, x_uSize};
    X_ReadUnsafe(pData, uSize);
}

void ByteBuffer::WriteBytes(const void *pData, U32 uSize) noexcept {
    if (!uSize)
        return;
    auto pChunk = X_Reserve(uSize);
    X_WriteUnsafe(pChunk, pData, uSize);
}

#ifdef BYTEBUFFER_NEED_AS_MUCH
U32 ByteBuffer::DiscardAsMuch(U32 uSize) noexcept {
    uSize = std::min(uSize, x_uSize);
    X_DiscardUnsafe(uSize);
    return uSize;
}

U32 ByteBuffer::PeekAsMuch(void *pData, U32 uSize) const noexcept {
    uSize = std::min(uSize, x_uSize);
    X_PeekUnsafe(pData, uSize);
    return uSize;
}

U32 ByteBuffer::ReadAsMuch(void *pData, U32 uSize) noexcept {
    uSize = std::min(uSize, x_uSize);
    X_ReadUnsafe(pData, uSize);
    return uSize;
}

U32 ByteBuffer::WriteAsMuch(const void *pData, U32 uSize) noexcept {
    X_Reserve(uSize);
    X_WriteUnsafe(pData, uSize);
    return uSize;
}
#endif

void ByteBuffer::Splice(ByteBuffer &vBuf) noexcept {
    x_uSize += std::exchange(vBuf.x_uSize, 0);
    x_liChunks.Splice(vBuf.x_liChunks);
}

void ByteBuffer::Append(const ByteBuffer &vBuf) noexcept {
    if (vBuf.IsEmpty())
        return;
    auto pChunk = X_Reserve(vBuf.GetSize());
    for (auto pOther = vBuf.x_liChunks.GetHead(); pOther != vBuf.x_liChunks.GetNil(); pOther = pOther->GetNext())
        X_WriteUnsafe(pChunk, pOther->GetReader(), pOther->GetReadable());
}

ByteBuffer ByteBuffer::Extract(U32 uSize) {
    if (!uSize)
        return {};
    if (x_uSize < uSize)
        throw ExnNoEnoughData {uSize, x_uSize};
    return X_ExtractUnsafe(uSize);
}

#ifdef BYTEBUFFER_NEED_AS_MUCH
ByteBuffer ByteBuffer::ExtractAsMuch(U32 uSize) noexcept {
    uSize = std::min(uSize, x_uSize);
    return X_ExtractUnsafe(uSize);
}
#endif

String ByteBuffer::ReadUtf8() {
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

void ByteBuffer::WriteUtf8(const String &sData) {
    auto uLen = ConvertWideToUtf8(sData);
    auto pChunk = X_Reserve(sizeof(U16) + uLen);
    X_WriteUnsafe(pChunk, &uLen, sizeof(U16));
    if (uLen)
        X_WriteUnsafe(pChunk, g_szUtf8Buf, uLen);
}
