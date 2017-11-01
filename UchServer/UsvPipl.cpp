#include "Common.hpp"

#include "Event.hpp"
#include "Usv.hpp"
#include "UsvPipl.hpp"

UsvPipl::UsvPipl(SOCKET hSocket) : Pipeline(*this, hSocket) {
    Usv::Bus().PostEvent(event::EvLog {
        static_cast<String>(GetLower().GetRemoteSockName()) + L" has connected."
    });
}

void UsvPipl::OnPacket(Buffer vPakBuf) noexcept {
    using namespace protocol;
    auto byId = vPakBuf.Read<Byte>();
    switch (byId) {
    case clsv::kExit:
        vPakBuf.Read<EvcExit>();
        Usv::Sto().Exit(x_sUser);
        Close();
        break;
    case clsv::kLoginReq:
        {
            EvcLoginReq e;
            vPakBuf >> e;
            auto pAddr = reinterpret_cast<sockaddr_in *>(&e.vUser.vAddr.vSockAddr);
            auto pRemo = reinterpret_cast<const sockaddr_in *>(&GetLower().GetRemoteSockName().vSockAddr);
            pAddr->sin_addr = pRemo->sin_addr;
            auto &&e_ = Usv::Sto().Login(e);
            if (e_.bSuccess)
                x_sUser = e.vUser.sName;
            PostPacket(std::move(e_));
        }
        break;
    case clsv::kRegisReq:
        PostPacket(Usv::Sto().Register(vPakBuf.Read<EvcRegisReq>()));
        break;
    case clsv::kRecoUserReq:
        PostPacket(Usv::Sto().RecoUser(vPakBuf.Read<EvcRecoUserReq>()));
        break;
    case clsv::kRecoPassReq:
        PostPacket(Usv::Sto().RecoPass(vPakBuf.Read<EvcRecoPassReq>()));
        break;
    case clsv::kMessageTo:
        Usv::Sto().MessageTo(x_sUser, vPakBuf.Read<EvcMessageTo>());
        break;
    default:
        assert(false); // wtf?
    }
}

void UsvPipl::OnPassivelyClose() noexcept {}

void UsvPipl::OnForciblyClose() noexcept {}

void UsvPipl::OnFinalize() noexcept {
    Usv::Cmg()->X_Unregister(this);
}
