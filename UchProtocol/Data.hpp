#pragma once

#include "Common.hpp"

namespace protocol {
#   define UEV_NAME OnlineUser
#   define UEV_NOTEV
#   define UEV_MEMBERS UEV_VAL(String, sName) UEV_END(SockName, vAddr)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_NAME ChatMessage
#   define UEV_NOTEV
#   define UEV_MEMBERS UEV_VAL(String, sFrom) UEV_END(String, sMessage)
#   include "../UchCommon/GenEvent.inl"

}
