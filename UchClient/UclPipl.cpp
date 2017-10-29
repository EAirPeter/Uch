#include "Common.hpp"

#include "Event.hpp"
#include "Ucl.hpp"
#include "UclPipl.hpp"

void UclPipl::OnPacket(Buffer vPakBuf) noexcept {
#define ONPAK(id_, type_) { case id_: Ucl::Bus().PostEvent(vPakBuf.Read<type_>()); break; }
    using namespace protocol;
    auto byId = vPakBuf.Read<Byte>();
    switch (byId) {
        ONPAK(svcl::kPulse, EvsPulse);
        ONPAK(svcl::kExit, EvsExit);
        ONPAK(svcl::kLoginRes, EvsLoginRes);
        ONPAK(svcl::kRegisRes, EvsRegisRes);
        ONPAK(svcl::kRecoUserRes, EvsRecoUserRes);
        ONPAK(svcl::kRecoPassRes, EvsRecoPassRes);
        ONPAK(svcl::kListRes, EvsListRes);
        ONPAK(svcl::kMessageFrom, EvsMessageFrom);
        ONPAK(svcl::kP2pFrom, EvsP2pFrom);
    default:
        assert(false); // wtf???
    }
#undef ONPAK
}

void UclPipl::OnPassivelyClose() noexcept {}

void UclPipl::OnForciblyClose() noexcept {
    Ucl::Bus().PostEvent(event::EvDisconnectFromServer {});
}

void UclPipl::OnFinalize() noexcept {}
