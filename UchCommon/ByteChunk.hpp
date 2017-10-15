#pragma once

#include "Common.hpp"

#include "IntrList.hpp"

class ByteChunk;

struct ChunkIoContext : public OVERLAPPED {
    void (*pfnIoCallback)(void *pParam, DWORD dwRes, USize uDone, ByteChunk *pChunk) noexcept;
};

// Implemented only low level operations.
// Would not check ranges.
class ByteChunk : public ChunkIoContext, public IntrListNode<ByteChunk> {
private:
    constexpr static USize x_kuAlignUnit = alignof(Byte *);

public:
    static inline std::unique_ptr<ByteChunk> MakeUnique(USize uCapacity, USize uAlign = x_kuAlignUnit) noexcept {
        assert(!(uAlign & (uAlign - 1)));
        assert(uAlign >= x_kuAlignUnit);
        assert(uCapacity);
        auto pChunk = reinterpret_cast<ByteChunk *>(
            ::_aligned_offset_malloc(sizeof(ByteChunk) + uCapacity, uAlign, sizeof(ByteChunk))
        );
        return std::unique_ptr<ByteChunk>(
            new(pChunk) ByteChunk(reinterpret_cast<Byte *>(&pChunk[1]) + uCapacity)
        );
    }

    static inline void operator delete(void *pChunk) noexcept {
        ::_aligned_free(pChunk);
    }

private:
    constexpr ByteChunk(Byte *pEnd) noexcept :
        ChunkIoContext {}, IntrListNode(),
        x_pReader(reinterpret_cast<Byte *>(&this[1])),
        x_pWriter(reinterpret_cast<Byte *>(&this[1])),
        x_pEnd(pEnd)
    {}

    ByteChunk(const ByteChunk &) = delete;
    ByteChunk(ByteChunk &&) = delete;

    ByteChunk &operator =(const ByteChunk &) = delete;
    ByteChunk &operator =(ByteChunk &&) = delete;

public:
    constexpr USize GetCapacity() const noexcept {
        return static_cast<USize>(x_pEnd - GetData());
    }

    constexpr USize GetReaderIdx() const noexcept {
        return static_cast<USize>(x_pReader - GetData());
    }

    constexpr USize GetWriterIdx() const noexcept {
        return static_cast<USize>(x_pWriter - GetData());
    }

    constexpr bool IsPrependable() const noexcept {
        return GetData() != x_pReader;
    }

    constexpr bool IsReadable() const noexcept {
        return x_pReader != x_pWriter;
    }

    constexpr bool IsWritable() const noexcept {
        return x_pWriter != x_pEnd;
    }

    constexpr USize GetPrependable() const noexcept {
        return static_cast<USize>(x_pReader - GetData());
    }

    constexpr USize GetReadable() const noexcept {
        return static_cast<USize>(x_pWriter - x_pReader);
    }
    
    constexpr USize GetWritable() const noexcept {
        return static_cast<USize>(x_pEnd - x_pWriter);
    }

    constexpr Byte *GetData() noexcept {
        return reinterpret_cast<Byte *>(&this[1]);
    }

    constexpr const Byte *GetData() const noexcept {
        return reinterpret_cast<const Byte *>(&this[1]);
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
        x_pReader = GetData();
        x_pWriter = GetData();
    }

    constexpr void ToIdx(USize uIdx) noexcept {
        x_pReader = GetData() + uIdx;
        x_pWriter = GetData() + uIdx;
    }

    constexpr void ToEnd() noexcept {
        x_pReader = x_pEnd;
        x_pWriter = x_pEnd;
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

    inline void Suck(ByteChunk &vChunk, USize uSize) noexcept {
        x_pReader -= uSize;
        vChunk.x_pWriter -= uSize;
        std::memcpy(x_pReader, vChunk.x_pWriter, uSize);
    }

    inline void Transfer(ByteChunk &vChunk, USize uSize) noexcept {
        std::memcpy(x_pWriter, vChunk.x_pReader, uSize);
        x_pWriter += uSize;
        vChunk.x_pReader += uSize;
    }

private:
    Byte *x_pReader;
    Byte *x_pWriter;
    Byte *const x_pEnd;

};
