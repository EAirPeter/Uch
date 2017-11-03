#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

namespace event {
    enum Id : Byte {
        kMessage = protocol::kEnd,
        kListUon,
        kListUff,
        kEnd,
    };

#   define UEV_ID kMessage
#   define UEV_NAME EvMessage
#   define UEV_MEMBERS UEV_END(protocol::ChatMessage, vMsg)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID kListUon
#   define UEV_NAME EvListUon
#   define UEV_MEMBERS UEV_END(std::vector<String>, vecUon)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID kListUff
#   define UEV_NAME EvListUff
#   define UEV_MEMBERS UEV_END(std::vector<String>, vecUff)
#   include "../UchCommon/GenEvent.inl"

}
