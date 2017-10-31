#pragma once

#include "Common.hpp"

#include <nana/gui/widgets/listbox.hpp>

namespace protocol {
#   define UEV_NAME OnlineUser
#   define UEV_NOTEV
#   define UEV_MEMBERS UEV_VAL(String, sName) UEV_END(SockName, vAddr)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_NAME ChatMessage
#   define UEV_NOTEV
#   define UEV_MEMBERS UEV_VAL(String, sFrom) UEV_END(String, sMessage)
#   include "../UchCommon/GenEvent.inl"

    inline nana::listbox::oresolver &operator <<(nana::listbox::oresolver &vOr, const ChatMessage &vMsg) {
        return vOr << vMsg.sFrom << vMsg.sMessage;
    }

}
