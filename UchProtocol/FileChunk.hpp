#pragma once

#include "Common.hpp"

#include "../UchCommon/ByteChunk.hpp"

namespace ImplFileChunk {
    constexpr U32 kuSize = 32 << 20;
    constexpr USize kuAlign = 4 << 10;

    struct FileChunk : ByteChunk<kuSize> {
        friend struct AlignHelper;
    };

    struct AlignHelper {
        constexpr static USize kuAlignOff = offsetof(FileChunk, x_abyData);
    };

    using FileChunkPool = Pool<FileChunk, AlignHelper::kuAlignOff, kuAlign>;

}

using ImplFileChunk::FileChunk;
using ImplFileChunk::FileChunkPool;
