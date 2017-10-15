#pragma once

#include "Common.hpp"

template<USize kuCapacity>
class StaticChunk {
public:
    constexpr StaticChunk() noexcept = default;
    StaticChunk(const StaticChunk &) = delete;
    StaticChunk(StaticChunk &&) = delete;

    StaticChunk &operator =(const StaticChunk &) = delete;
    StaticChunk &operator =(StaticChunk &&) = delete;

public:
    constexpr USize GetCapacity() const noexcept {
        return kuCapacity;
    }

    constexpr USize GetReaderIdx() const noexcept {
        return static_cast<USize>(x_pReader - x_abyData);
    }

    constexpr USize GetWriterIdx() const noexcept {
        return static_cast<USize>(x_pWriter - x_abyData);
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

    constexpr USize GetPrependable() const noexcept {
        return static_cast<USize>(x_pReader - x_abyData);
    }

    constexpr USize GetReadable() const noexcept {
        return static_cast<USize>(x_pWriter - x_pReader);
    }

    constexpr USize GetWritable() const noexcept {
        return kuCapacity - static_cast<USize>(x_pWriter - x_abyData);
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

    constexpr void ToIdx(USize uIdx) noexcept {
        x_pReader = x_abyData + uIdx;
        x_pWriter = x_abyData + uIdx;
    }

    constexpr void ToEnd() noexcept {
        x_pReader = x_abyData + kuCapacity;
        x_pWriter = x_abyData + kuCapacity;
    }

    constexpr void Discard(USize uSize) noexcept {
        x_pReader += uSize;
    }

    constexpr void Skip(USize uSize) noexcept {
        x_pWriter += uSize;
    }

    inline void Prepend(const void *pData, USize uSize) noexcept {
        x_pReader -= uSize;
        std::memcpy(x_pReader, pData, uSize);
    }

    inline void Peek(void *pData, USize uSize) const noexcept {
        std::memcpy(pData, x_pReader, uSize);
    }

    inline void Read(void *pData, USize uSize) noexcept {
        std::memcpy(pData, x_pReader, uSize);
        x_pReader += uSize;
    }

    inline void Write(const void *pData, USize uSize) noexcept {
        std::memcpy(x_pWriter, pData, uSize);
        x_pWriter += uSize;
    }

    inline void Fill(Byte byVal, USize uSize) noexcept {
        std::memset(x_pWriter, static_cast<int>(byVal), uSize);
        x_pWriter += uSize;
    }

    inline void Suck(StaticChunk &vChunk, USize uSize) noexcept {
        x_pReader -= uSize;
        vChunk.x_pWriter -= uSize;
        std::memcpy(x_pReader, vChunk.x_pWriter, uSize);
    }

    inline void Transfer(StaticChunk &vChunk, USize uSize) noexcept {
        std::memcpy(x_pWriter, vChunk.x_pReader, uSize);
        x_pWriter += uSize;
        vChunk.x_pReader += uSize;
    }

private:
    Byte x_abyData[kuCapacity];
    Byte *x_pReader = x_abyData;
    Byte *x_pWriter = x_abyData;

};
