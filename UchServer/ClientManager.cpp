#include "Common.hpp"

#include "ClientManager.hpp"
#include "Usv.hpp"

ClientManager::ClientManager() :
    x_vLis (
        *this,
        CreateBoundSocket(MakeSockName(Usv::Cfg()[UsvCfg::kszHost], Usv::Cfg()[UsvCfg::kszPort]), true),
        SOMAXCONN
    )
{
    AssignToIoGroup(Usv::Iog());
    Usv::Bus().Register(*this);
}

ClientManager::~ClientManager() {
    Usv::Bus().Unregister(*this);
}

void ClientManager::AssignToIoGroup(IoGroup &vIoGroup) {
    x_vLis.AssignToIoGroup(vIoGroup);
}

void ClientManager::OnAccept(SOCKET hAccepted) noexcept {
    auto upPipl = std::make_unique<UsvPipl>(hAccepted);
    RAII_LOCK(x_mtx);
    x_setPipls.emplace(upPipl.get());
    x_vLis.RegisterPipeline(hAccepted, std::move(upPipl));
}

void ClientManager::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bDone = true;
    x_cv.WakeOne();
}

void ClientManager::Shutdown() noexcept {
    {
        RAII_LOCK(x_mtx);
        for (auto &p : x_setPipls) {
            p->PostPacket(protocol::EvsExit {L"Server shutting down"});
            p->Shutdown();
        }
    }
    x_vLis.Shutdown();
}

void ClientManager::Wait() noexcept {
    RAII_LOCK(x_mtx);
    while (!x_bDone)
        x_cv.Wait(x_mtx);
}

void ClientManager::OnEvent(protocol::EvsNewUser &e) noexcept {
    RAII_LOCK(x_mtx);
    for (auto pPipl : x_setPipls)
        pPipl->PostPacket(e);
}

std::vector<protocol::ChatMessage> ClientManager::ExtractMsg(const String &sUser) {
    RAII_LOCK(x_mtx);
    auto it = x_mapMsgs.find(sUser);
    if (it == x_mapMsgs.end())
        return {};
    auto &&vec = std::move(it->second);
    x_mapMsgs.erase(it);
    return std::move(vec);
}

void ClientManager::X_Unregister(UsvPipl *pPipl) noexcept {
    x_pIoGroup->PostJob(&ClientManager::X_ImplUnregister, this, pPipl);
}

void ClientManager::X_ImplUnregister(UsvPipl *pPipl) noexcept {
    RAII_LOCK(x_mtx);
    x_setPipls.erase(pPipl);
    x_vLis.RemovePipeline(pPipl->GetLower().GetNative());
}
