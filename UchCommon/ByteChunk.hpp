#pragma once

#include "Common.hpp"

#include "IntrList.hpp"

struct ChunkIoContext : public OVERLAPPED {
    void (*pfnIoCallback)(void *pParam, DWORD dwRes, U32 uDone, ChunkIoContext *pCtx) noexcept;
};

template<U32 kuCapacity>
class ByteChunk : public ChunkIoContext {
public:
    constexpr static auto kCapacity = kuCapacity;

public:
    constexpr ByteChunk() noexcept = default;
    ByteChunk(const ByteChunk &) = delete;
    ByteChunk(ByteChunk &&) = delete;

    constexpr ByteChunk(U32 uOffset) noexcept :
        ChunkIoContext {},
        x_pReader(x_abyData + uOffset),
        x_pWriter(x_abyData + uOffset)
    {}

    ByteChunk &operator =(const ByteChunk &) = delete;
    ByteChunk &operator =(ByteChunk &&) = delete;

public:
    constexpr U32 GetReaderIdx() const noexcept {
        return static_cast<U32>(x_pReader - x_abyData);
    }

    constexpr U32 GetWriterIdx() const noexcept {
        return static_cast<U32>(x_pWriter - x_abyData);
    }

    constexpr bool IsReadable() const noexcept {
        return x_pReader != x_pWriter;
    }

    constexpr bool IsWritable() const noexcept {
        return x_pWriter != x_abyData + kuCapacity;
    }

    constexpr U32 GetReadable() const noexcept {
        return static_cast<U32>(x_pWriter - x_pReader);
    }

    constexpr U32 GetWritable() const noexcept {
        return kuCapacity - static_cast<U32>(x_pWriter - x_abyData);
    }

    constexpr Byte *GetData() noexcept {
        return x_abyData;
    }

    constexpr const Byte *GetData() const noexcept {
        return x_abyData;
    }

    constexpr Byte *GetReader() noexcept {
        return x_pReader;
    }

    constexpr const Byte *GetReader() const noexcept {
        return x_pReader;
    }

    constexpr Byte *GetWriter() noexcept {
        return x_pWriter;
    }

    constexpr const Byte *GetWriter() const noexcept {
        return x_pWriter;
    }

public:
    constexpr void ToBegin() noexcept {
        x_pReader = x_abyData;
        x_pWriter = x_abyData;
    }

    constexpr void ToIdx(U32 uIdx) noexcept {
        x_pReader = x_abyData + uIdx;
        x_pWriter = x_abyData + uIdx;
    }

    constexpr void ToEnd() noexcept {
        x_pReader = x_abyData + kuCapacity;
        x_pWriter = x_abyData + kuCapacity;
    }

    constexpr void SetReaderIdx(U32 uIdx) noexcept {
        x_pReader = x_abyData + uIdx;
    }

    constexpr void SetWriterIdx(U32 uIdx) noexcept {
        x_pWriter = x_abyData + uIdx;
    }

    constexpr void IncReader(U32 uBy) noexcept {
        x_pReader += uBy;
    }

    constexpr void IncWriter(U32 uBy) noexcept {
        x_pWriter += uBy;
    }

    constexpr void DecReader(U32 uBy) noexcept {
        x_pReader -= uBy;
    }

    constexpr void DecWriter(U32 uBy) noexcept {
        x_pWriter -= uBy;
    }

    inline void Peek(void *pData, U32 uSize) const noexcept {
        std::memcpy(pData, x_pReader, uSize);
    }

    inline void Read(void *pData, U32 uSize) noexcept {
        std::memcpy(pData, x_pReader, uSize);
        x_pReader += uSize;
    }

    inline void Write(const void *pData, U32 uSize) noexcept {
        std::memcpy(x_pWriter, pData, uSize);
        x_pWriter += uSize;
    }

    inline void Fill(Byte byVal, U32 uSize) noexcept {
        std::memset(x_pWriter, static_cast<int>(byVal), uSize);
        x_pWriter += uSize;
    }

    inline void Transfer(ByteChunk &vChunk, U32 uSize) noexcept {
        std::memcpy(x_pWriter, vChunk.x_pReader, uSize);
        x_pWriter += uSize;
        vChunk.x_pReader += uSize;
    }

protected:
    Byte alignas(MEMORY_ALLOCATION_ALIGNMENT) x_abyData[kuCapacity];
    Byte *x_pReader = x_abyData;
    Byte *x_pWriter = x_abyData;

};

template<U32 kuCapacity>
struct LinkedChunk : ByteChunk<kuCapacity>, IntrListNode<LinkedChunk<kuCapacity>> {};
