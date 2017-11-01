#include "Common.hpp"

#include "Ucl.hpp"
#include "UclPipl.hpp"

UclPipl::UclPipl() :
    Pipeline(*this, CreateConnectedSocket(
        MakeSockName(Ucl::Cfg()[UclCfg::kszHost], Ucl::Cfg()[UclCfg::kszPort])
    ))
{
    AssignToIoGroup(Ucl::Iog());
}

void UclPipl::OnPacket(Buffer vPakBuf) noexcept {
#define ONPAK(id_, type_) { case id_: Ucl::Bus().PostEvent(vPakBuf.Read<type_>()); break; }
    using namespace protocol;
    auto byId = vPakBuf.Read<Byte>();
    switch (byId) {
        ONPAK(svcl::kExit, EvsExit);
        ONPAK(svcl::kLoginRes, EvsLoginRes);
        ONPAK(svcl::kRegisRes, EvsRegisRes);
        ONPAK(svcl::kRecoUserRes, EvsRecoUserRes);
        ONPAK(svcl::kRecoPassRes, EvsRecoPassRes);
        ONPAK(svcl::kNewUser, EvsNewUser);
    default:
        assert(false); // wtf???
    }
#undef ONPAK
}

void UclPipl::OnPassivelyClose() noexcept {}

void UclPipl::OnForciblyClose() noexcept {}

void UclPipl::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bDone = true;
    x_cv.WakeOne();
}

void UclPipl::Wait() noexcept {
    RAII_LOCK(x_mtx);
    while (!x_bDone)
        x_cv.Wait(x_mtx);
}
