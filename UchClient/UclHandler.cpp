#include "Common.hpp"

#include "Ucl.hpp"
#include "UclHandler.hpp"

#include <nana/gui.hpp>

UclHandler::UclHandler() {
    Ucl::Bus().Register(*this);
}

UclHandler::~UclHandler() {
    Ucl::Bus().Unregister(*this);
}

void UclHandler::OnEvent(event::EvDisconnectFromServer &e) noexcept {
    UNREFERENCED_PARAMETER(e);
    nana::API::exit_all();
}
