#pragma once

#include "Common.hpp"

namespace event {
    enum Id : Byte {
        kLog = protocol::kEnd,
        kEnd,
    };

#   define UEV_ID kLog
#   define UEV_NAME EvLog
#   define UEV_MEMBERS UEV_END(String, sWhat)
#   include "../UchCommon/GenEvent.inl"

}
