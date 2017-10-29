#pragma once

#include "Common.hpp"

#include "Event.hpp"

class UclHandler : HandlerBase<event::EvDisconnectFromServer> {
    friend class Ucl;

public:
    void OnEvent(event::EvDisconnectFromServer &e) noexcept override final;

private:
    UclHandler();
    ~UclHandler();

};
