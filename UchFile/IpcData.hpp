#pragma once

#include "../UchCommon/Common.hpp"

namespace uchfile {
    struct IpcData {
        U32 uzSentSec;
        U32 uzRcvdSec;
        U64 uzFile;
    };

}
