#pragma once

#include "Common.hpp"

#include "IoGroup.hpp"
#include "SockName.hpp"
#include "Sync.hpp"

template<class tUpper, class tPipl>
class Listener {
public:
    using Upper = tUpper;
    using Pipl = tPipl;

public:
    Listener(Upper &vUpper, const SockName &vSnListen, int nMaxConn) :
        x_vUpper(vUpper),
        x_hSocket(CreateTcpSocket()),
        x_vSnListen(vSnListen),
        x_nMaxConn(nMaxConn)
    {
        int nRes;
        x_hEvent = WSACreateEvent();
        if (x_hEvent == WSA_INVALID_EVENT) {
            nRes = WSAGetLastError();
            closesocket(x_hSocket);
            throw ExnWsa(nRes);
        }
        nRes = WSAEventSelect(x_hSocket, x_hEvent, FD_ACCEPT);
        if (nRes) {
            nRes = WSAGetLastError();
            WSACloseEvent(x_hEvent);
            closesocket(x_hSocket);
            throw ExnWsa(nRes);
        }
        nRes = bind(x_hSocket, &x_vSnListen.vSockAddr, static_cast<int>(x_vSnListen.nSockLen));
        if (nRes) {
            nRes = WSAGetLastError();
            WSACloseEvent(x_hEvent);
            closesocket(x_hSocket);
            throw ExnWsa(nRes);
        }
    }

    ~Listener() {
        WSACloseEvent(x_hEvent);
        closesocket(x_hSocket);
    }

public:
    constexpr Upper &GetUpper() noexcept {
        return x_vUpper;
    }

    constexpr SOCKET GetNative() const noexcept {
        return x_hSocket;
    }

    constexpr const SockName &GetListenSockName() const noexcept {
        return x_vSnListen;
    }

public:
    void AssignToIoGroup(IoGroup &vIoGroup) {
        RAII_LOCK(x_mtx);
        auto nRes = listen(x_hSocket, x_nMaxConn);
        if (nRes)
            throw ExnWsa();
        vIoGroup.RegisterNetev(*this, x_hSocket, x_hEvent);
        x_pIoGroup = &vIoGroup;
    }

    void Shutdown() noexcept {
        RAII_LOCK(x_mtx);
        x_bStopping = true;
        if (x_pIoGroup) {
            x_pIoGroup->UnregisterNetev();
            x_pIoGroup = nullptr;
        }
        if (x_mapPipls.empty())
            x_vUpper.OnFinalize();
    }

    bool OnNetev(WSANETWORKEVENTS &vNetev) noexcept {
        if (vNetev.lNetworkEvents & FD_ACCEPT) {
            if (vNetev.iErrorCode[FD_ACCEPT_BIT]) {
                // something happened
                return true;
            }
            auto hAccepted = WSAAccept(x_hSocket, nullptr, nullptr, nullptr, 0);
            if (hAccepted == INVALID_SOCKET) {
                // something happened
                return true;
            }
            RAII_LOCK(x_mtx);
            x_vUpper.OnAccept(hAccepted);
            return true;
        }
        assert(false);
        return false;
    }

    inline void RegisterPipeline(SOCKET hSocket, std::unique_ptr<Pipl> upPipl) {
        RAII_LOCK(x_mtx);
        upPipl->AssignToIoGroup(*x_pIoGroup);
        x_mapPipls.emplace(hSocket, std::move(upPipl));
    }

    inline void RemovePipeline(SOCKET hSocket) {
        RAII_LOCK(x_mtx);
        x_mapPipls.erase(hSocket);
        if (x_bStopping && x_mapPipls.empty())
            x_vUpper.OnFinalize();
    }

private:
    Upper &x_vUpper;

    RecursiveMutex x_mtx;

    bool x_bStopping = false;
    IoGroup *x_pIoGroup = nullptr;

    SOCKET x_hSocket;
    WSAEVENT x_hEvent;
    const SockName x_vSnListen;
    const int x_nMaxConn;

    std::unordered_map<SOCKET, std::unique_ptr<Pipl>> x_mapPipls;

};
