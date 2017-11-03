#include "Common.hpp"

#include "Event.hpp"
#include "PeerManager.hpp"
#include "Ucl.hpp"

PeerManager::PeerManager() : 
    x_vLis(
        *this, 
        CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszLisHost], L"0"), true),
        SOMAXCONN
    )
{
    AssignToIoGroup(Ucl::Iog());
    Ucl::Bus().Register(*this);
}

PeerManager::~PeerManager() {
    Ucl::Bus().Unregister(*this);
}

void PeerManager::AssignToIoGroup(IoGroup &vIoGroup) {
    x_vLis.AssignToIoGroup(vIoGroup);
    x_pIoGroup = &vIoGroup;
}

void PeerManager::OnAccept(SOCKET hAccepted) noexcept {
    x_vLis.RegisterPipeline(hAccepted, std::make_unique<UccPipl>(hAccepted));
}

void PeerManager::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bDone = true;
    x_cv.WakeOne();
}

void PeerManager::Shutdown() noexcept {
    auto sUser = Ucl::Usr();
    {
        RAII_LOCK(x_mtx);
        x_bStopping = true;
        for (auto &p : x_mapUon) {
            p.second->PostPacket(protocol::EvpExit {});
            p.second->Shutdown();
        }
    }
    x_vLis.Shutdown();
}

void PeerManager::Wait() noexcept {
    RAII_LOCK(x_mtx);
    while (!x_bDone && !x_mapPipls.empty())
        x_cv.Wait(x_mtx);
}

void PeerManager::OnEvent(protocol::EvsExit &e) noexcept {
    UNREFERENCED_PARAMETER(e);
    Shutdown();
}

void PeerManager::OnEvent(protocol::EvsNewUser &e) noexcept {
    RAII_LOCK(x_mtx);
    x_setUff.emplace(e.sUser);
    X_UpdateFfline();
}

void PeerManager::X_Register(const String &sUser, UccPipl *pPipl) {
    RAII_LOCK(x_mtx);
    if (x_mapUon.emplace(sUser, pPipl).second)
        x_setUff.erase(sUser);
    X_UpdateOnline();
    X_UpdateFfline();
}

void PeerManager::X_Unregister(const String &sUser, UccPipl *pPipl) noexcept {
    x_pIoGroup->PostJob(&PeerManager::X_ImplUnregister, this, sUser, pPipl);
}

void PeerManager::X_ImplUnregister(const String &sUser, UccPipl *pPipl) noexcept {
    RAII_LOCK(x_mtx);
    if (x_mapUon.erase(sUser))
        x_setUff.emplace(sUser);
    auto hSocket = pPipl->GetLower().GetNative();
    auto it_ = x_mapPipls.find(hSocket);
    if (it_ != x_mapPipls.end())
        x_mapPipls.erase(it_);
    else
        x_vLis.RemovePipeline(hSocket);
    X_UpdateOnline();
    X_UpdateFfline();
}

void PeerManager::SetOnline(const std::vector<protocol::OnlineUser> &vecOnline) {
    RAII_LOCK(x_mtx);
    for (auto &vUsr : vecOnline) {
        auto hSocket = CreateConnectedSocket(vUsr.vAddr, true);
        auto upPipl = std::make_unique<UccPipl>(hSocket, vUsr.sName);
        upPipl->AssignToIoGroup(*x_pIoGroup);
        upPipl->PostPacket(protocol::EvpAuth {Ucl::Usr()});
        x_mapUon.emplace(vUsr.sName, upPipl.get());
        x_mapPipls.emplace(hSocket, std::move(upPipl));
    }
    X_UpdateOnline();
}

void PeerManager::SetFfline(const std::vector<String> &vecFfline) {
    RAII_LOCK(x_mtx);
    x_setUff.clear();
    x_setUff.insert(vecFfline.begin(), vecFfline.end());
    X_UpdateFfline();
}

UccPipl &PeerManager::operator[](const String &sUser) {
    RAII_LOCK(x_mtx);
    return *x_mapUon.at(sUser);
}

void PeerManager::X_UpdateOnline() {
    std::vector<String> vecUon;
    vecUon.reserve(x_mapUon.size());
    for (auto &p : x_mapUon)
        vecUon.emplace_back(p.first);
    Ucl::Bus().PostEvent(event::EvListUon {std::move(vecUon)});
}

void PeerManager::X_UpdateFfline() {
    Ucl::Bus().PostEvent(event::EvListUff {
        std::vector<String> {x_setUff.begin(), x_setUff.end()}
    });
}
