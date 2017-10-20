#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "Pool.hpp"

template<U32 kuCapacity>
class StaticChunk : public ChunkIoContext {
public:
    static inline void *operator new(USize) noexcept {
        return Pool<StaticChunk>::Instance().Alloc();
    }

    static inline void operator delete(void *pChunk) noexcept {
        Pool<StaticChunk>::Instance().Dealloc(reinterpret_cast<StaticChunk *>(pChunk));
    }

public:
    constexpr StaticChunk() noexcept = default;
    StaticChunk(const StaticChunk &) = delete;
    StaticChunk(StaticChunk &&) = delete;

    StaticChunk &operator =(const StaticChunk &) = delete;
    StaticChunk &operator =(StaticChunk &&) = delete;

public:
    constexpr U32 GetCapacity() const noexcept {
        return kuCapacity;
    }

    constexpr U32 GetReaderIdx() const noexcept {
        return static_cast<U32>(x_pReader - x_abyData);
    }

    constexpr U32 GetWriterIdx() const noexcept {
        return static_cast<U32>(x_pWriter - x_abyData);
    }

    constexpr bool IsPrependable() const noexcept {
        return x_abyData != x_pReader;
    }

    constexpr bool IsReadable() const noexcept {
        return x_pReader != x_pWriter;
    }

    constexpr bool IsWritable() const noexcept {
        return x_pWriter != x_abyData + kuCapacity;
    }

    constexpr U32 GetPrependable() const noexcept {
        return static_cast<U32>(x_pReader - x_abyData);
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

    constexpr void Discard(U32 uSize) noexcept {
        x_pReader += uSize;
    }

    constexpr void Skip(U32 uSize) noexcept {
        x_pWriter += uSize;
    }

    inline void Prepend(const void *pData, U32 uSize) noexcept {
        x_pReader -= uSize;
        std::memcpy(x_pReader, pData, uSize);
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

    inline void Suck(StaticChunk &vChunk, U32 uSize) noexcept {
        x_pReader -= uSize;
        vChunk.x_pWriter -= uSize;
        std::memcpy(x_pReader, vChunk.x_pWriter, uSize);
    }

    inline void Transfer(StaticChunk &vChunk, U32 uSize) noexcept {
        std::memcpy(x_pWriter, vChunk.x_pReader, uSize);
        x_pWriter += uSize;
        vChunk.x_pReader += uSize;
    }

private:
    Byte x_abyData[kuCapacity];
    Byte *x_pReader = x_abyData;
    Byte *x_pWriter = x_abyData;

};
