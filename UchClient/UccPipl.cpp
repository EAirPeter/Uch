#include "Common.hpp"

#include "Event.hpp"
#include "FileRecv.hpp"
#include "UccPipl.hpp"
#include "Ucl.hpp"

#include <nana/gui.hpp>

UccPipl::UccPipl(SOCKET hSocket) : Pipeline(*this, hSocket) {}

UccPipl::UccPipl(SOCKET hSocket, const String &sUser) : Pipeline(*this, hSocket), x_sUser(sUser) {}

void UccPipl::OnPacket(Buffer vPakBuf) noexcept {
    using namespace protocol;
    auto byId = vPakBuf.Read<Byte>();
    switch (byId) {
    case p2pchat::kExit:
        vPakBuf.Read<EvpExit>();
        Close();
        break;
    case p2pchat::kAuth:
        x_sUser = vPakBuf.Read<EvpAuth>().sName;
        Ucl::Pmg()->X_Register(x_sUser, this);
        break;
    case p2pchat::kMessage:
        Ucl::Bus().PostEvent(event::EvMessage {ChatMessage {x_sUser, vPakBuf.Read<EvpMessage>().sMessage}});
        break;
    case p2pchat::kFileReq:
        // will leak of course, just let it go
        Ucl::Iog().PostJob(&FileRecv::Run, this, vPakBuf.Read<EvpFileReq>());
        break;
    case p2pchat::kFileRes:
        Ucl::Bus().PostEvent(vPakBuf.Read<EvpFileRes>());
        break;
    default:
        assert(false); // wtf???
    }
}

void UccPipl::OnPassivelyClose() noexcept {}

void UccPipl::OnForciblyClose() noexcept {}

void UccPipl::OnFinalize() noexcept {
    Ucl::Pmg()->X_Unregister(x_sUser, this);
}
