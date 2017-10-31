#pragma once

#include "Common.hpp"

#include "../UchCommon/Listener.hpp"
#include "../UchProtocol/Data.hpp"
#include "UccPipl.hpp"

class PeerManager : HandlerBase<protocol::EvsExit, protocol::EvsNewUser> {
    friend UccPipl;

public:
    PeerManager();
    ~PeerManager();

public:
    constexpr const SockName &GetListenSockName() const noexcept {
        return x_vLis.GetListenSockName();
    }

public:
    void AssignToIoGroup(IoGroup &vIoGroup);
    void OnAccept(SOCKET hAccepted) noexcept;
    void OnFinalize() noexcept;
    void Shutdown() noexcept;
    void Wait() noexcept;

    void OnEvent(protocol::EvsExit &e) noexcept override;
    void OnEvent(protocol::EvsNewUser &e) noexcept override;

private:
    void X_Register(const String &sUser, UccPipl *pPipl);
    void X_Unregister(const String &sUser, UccPipl *pPipl) noexcept;
    void X_ImplUnregister(const String &sUser, UccPipl *pPipl) noexcept;

public:
    void SetOnline(const std::vector<protocol::OnlineUser> &vecOnline);
    void SetFfline(const std::vector<String> &vecFfline);
    UccPipl &operator [](const String &sUser);

private:
    std::unordered_set<String> x_setUff;
    std::unordered_map<String, UccPipl *> x_mapUon;
    std::unordered_map<SOCKET, std::unique_ptr<UccPipl>> x_mapPipls;

    Listener<PeerManager, UccPipl> x_vLis;

    RWLock x_rwl;
    ConditionVariable x_cv;
    bool x_bDone = false;
    bool x_bStopping = false;

    IoGroup *x_pIoGroup = nullptr;

};
