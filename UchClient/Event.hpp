#pragma once

#include "Common.hpp"

#include "../UchProtocol/Event.hpp"

namespace event {
    enum Id : Byte {
        kBegin = protocol::kEnd,
        kDisconnectFromServer,
        kDisconnectFromPeer,
        kEnd,
    };
    
#   define UEV_ID kDisconnectFromServer
#   define UEV_NAME EvDisconnectFromServer
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID kDisconnectFromPeer
#   define UEV_NAME EvDisconnectFromPeer
#   include "../UchCommon/GenEvent.inl"

}
