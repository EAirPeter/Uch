#pragma once

#include "Common.hpp"

#include "../UchCommon/Listener.hpp"
#include "../UchProtocol/Data.hpp"
#include "UsvPipl.hpp"

class ClientManager :
    HandlerBase<
        protocol::EvsNewUser
    >
{
    friend UsvPipl;

public:
    ClientManager();
    ~ClientManager();

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

    void OnEvent(protocol::EvsNewUser &e) noexcept override;

public:
    std::vector<protocol::ChatMessage> ExtractMsg(const String &sUser);

private:
    void X_Unregister(UsvPipl *pPipl) noexcept;
    void X_ImplUnregister(UsvPipl *pPipl) noexcept;

private:
    std::unordered_set<UsvPipl *> x_setPipls;
    std::unordered_map<String, std::vector<protocol::ChatMessage>> x_mapMsgs;

    Listener<ClientManager, UsvPipl> x_vLis;

    Mutex x_mtx;
    ConditionVariable x_cv;
    bool x_bDone = false;
    bool x_bStopping = false;

    IoGroup *x_pIoGroup = nullptr;

};
