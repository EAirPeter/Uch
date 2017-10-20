#pragma once

#include "StaticChunk.hpp"

namespace ImplFileChunk {
    constexpr U32 kuSize = 32 << 20;
    constexpr USize kuAlign = 4 << 10;

    struct FileChunk : StaticChunk<kuSize> {
        friend struct AlignHelper;

        static inline void *operator new(USize) noexcept;

        static inline void operator delete(void *pChunk) noexcept;

    };

    struct AlignHelper {
        constexpr static USize kuAlignOff = offsetof(FileChunk, x_abyData);
    };

    using FileChunkPool = Pool<FileChunk, AlignHelper::kuAlignOff, kuAlign>;

    inline void *FileChunk::operator new(USize) noexcept {
        return FileChunkPool::Instance().Alloc();
    }

    inline void FileChunk::operator delete(void *pChunk) noexcept {
        FileChunkPool::Instance().Dealloc(reinterpret_cast<FileChunk *>(pChunk));
    }

}

using ImplFileChunk::FileChunk;
using ImplFileChunk::FileChunkPool;
