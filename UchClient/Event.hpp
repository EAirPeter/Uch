#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

namespace event {
    enum Id : Byte {
        kMessage = protocol::kEnd,
        kListUon,
        kListUff,
        kFileRecv,
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

#   define UEV_ID kFileRecv
#   define UEV_NAME EvFileRecv
#   define UEV_MEMBERS UEV_VAL(UccPipl *, pPipl) UEV_VAL(U64, uId) \
        UEV_VAL(String, sPath) UEV_VAL(U64, uSize) UEV_END(U16, uPort)
#   include "../UchCommon/GenEvent.inl"

}
