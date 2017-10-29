#pragma once

#include "Common.hpp"

#include "Wsa.hpp"

template<Byte kbyEventId, class tEvent>
struct EventBase {
    constexpr static Byte kEventId = kbyEventId;
    struct Handler {
        virtual void OnEvent(tEvent &e) noexcept = 0;
    };
};

template<class ...tvEvents>
struct HandlerBase : tvEvents::Handler... {};
