#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

struct FileRecv {
    static void Run(UccPipl *pPipl, const protocol::EvpFileReq &e);

};
