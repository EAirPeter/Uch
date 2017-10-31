#include "Common.hpp"

#include "Event.hpp"
#include "PeerManager.hpp"
#include "Ucl.hpp"

PeerManager::PeerManager() : 
    x_vLis(
        *this, 
        CreateBoundSocket(MakeSockName(L"0.0.0.0", L"0"), true),
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
    RAII_LOCK(x_rwl.WriteLock());
    x_bDone = true;
    x_cv.WakeOne();
}

void PeerManager::Shutdown() noexcept {
    auto sUser = Ucl::Usr();
    {
        RAII_LOCK(x_rwl.WriteLock());
        x_bStopping = true;
        x_vLis.Shutdown();
        for (auto &p : x_mapUon) {
            p.second->PostPacket(protocol::EvpExit {});
            p.second->Shutdown();
        }
    }
}

void PeerManager::Wait() noexcept {
    RAII_LOCK(x_rwl.WriteLock());
    while (!x_bDone && !x_mapPipls.empty())
        x_cv.Wait(x_rwl.WriteLock());
}

void PeerManager::OnEvent(protocol::EvsExit &e) noexcept {
    UNREFERENCED_PARAMETER(e);
    Shutdown();
}

void PeerManager::OnEvent(protocol::EvsNewUser &e) noexcept {
    RAII_LOCK(x_rwl.WriteLock());
    x_setUff.emplace(e.sUser);
}

void PeerManager::X_Register(const String &sUser, UccPipl *pPipl) {
    RAII_LOCK(x_rwl.WriteLock());
    x_mapUon.emplace(sUser, pPipl);
}

void PeerManager::X_Unregister(const String &sUser, UccPipl *pPipl) noexcept {
    x_pIoGroup->PostJob(&PeerManager::X_ImplUnregister, this, sUser, pPipl);
}

void PeerManager::X_ImplUnregister(const String &sUser, UccPipl *pPipl) noexcept {
    RAII_LOCK(x_rwl.WriteLock());
    x_mapUon.erase(sUser);
    auto hSocket = pPipl->GetLower().GetNative();
    auto it_ = x_mapPipls.find(hSocket);
    if (it_ != x_mapPipls.end())
        x_mapPipls.erase(it_);
    else
        x_vLis.RemovePipeline(hSocket);
}

void PeerManager::SetOnline(const std::vector<protocol::OnlineUser> &vecOnline) {
    std::vector<String> vecUon;
    vecUon.reserve(vecOnline.size());
    {
        RAII_LOCK(x_rwl.WriteLock());
        for (auto &vUsr : vecOnline) {
            auto hSocket = CreateConnectedSocket(vUsr.vAddr, true);
            auto upPipl = std::make_unique<UccPipl>(hSocket);
            upPipl->AssignToIoGroup(*x_pIoGroup);
            upPipl->PostPacket(protocol::EvpAuth {vUsr.sName});
            x_mapUon.emplace(vUsr.sName, upPipl.get());
            x_mapPipls.emplace(hSocket, std::move(upPipl));
            vecUon.emplace_back(vUsr.sName);
        }
    }
    Ucl::Bus().PostEvent(event::EvListUon {std::move(vecUon)});
}

void PeerManager::SetFfline(const std::vector<String> &vecFfline) {
    {
        RAII_LOCK(x_rwl.WriteLock());
        x_setUff.clear();
        x_setUff.insert(vecFfline.begin(), vecFfline.end());
    }
    Ucl::Bus().PostEvent(event::EvListUff {vecFfline});
}

UccPipl &PeerManager::operator[](const String &sUser) {
    RAII_LOCK(x_rwl.ReadLock());
    return *x_mapUon.at(sUser);
}

