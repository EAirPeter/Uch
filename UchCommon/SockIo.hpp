#pragma once

#include "Common.hpp"
#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IoGroup.hpp"
#include "SockName.hpp"
#include "Sync.hpp"

template<class tChunk>
struct ExnSockRead {
    int nError;
    tChunk *pChunk;
};

template<class tChunk>
struct ExnSockWrite {
    int nError;
    std::unique_ptr<tChunk> upChunk;
};

template<class tUpper>
class SockIo {
public:
    using Upper = tUpper;

public:
    SockIo() = delete;
    SockIo(const SockIo &) = delete;
    SockIo(SockIo &&) = delete;

    SockIo(Upper &vUpper, SOCKET hSocket) :
        x_vUpper(vUpper),
        x_hSocket(hSocket),
        x_vSnLocal(::GetLocalSockName(x_hSocket)), 
        x_vSnRemote(::GetRemoteSockName(x_hSocket))
    {}

    ~SockIo() {
        RAII_LOCK(x_mtx);
        closesocket(x_hSocket);
    }

    SockIo &operator =(const SockIo &) = delete;
    SockIo &operator =(SockIo &&) = delete;

public:
    constexpr Upper &GetUpper() noexcept {
        return x_vUpper;
    }

    constexpr SOCKET GetNative() const noexcept {
        return x_hSocket;
    }

    constexpr const SockName &GetLocalSockName() const noexcept {
        return x_vSnLocal;
    }

    constexpr const SockName &GetRemoteSockName() const noexcept {
        return x_vSnRemote;
    }

public:
    inline void AssignToIoGroup(IoGroup &vIoGroup) {
        RAII_LOCK(x_mtx);
        if (x_pIoGroup)
            throw ExnIllegalState();
        x_pTpIo = vIoGroup.RegisterIo(reinterpret_cast<HANDLE>(x_hSocket), this);
        x_pIoGroup = &vIoGroup;
    }

    // does not cancel pending send requests
    inline void Shutdown() noexcept {
        RAII_LOCK(x_mtx);
        X_Shutdown();
    }

    // cancels all pending requests
    inline void Close() noexcept {
        RAII_LOCK(x_mtx);
        X_Close();
    }

    template<class tChunk>
    inline void PostRead(tChunk *pChunk) {
        WSABUF vWsaBuf {
            static_cast<ULONG>(pChunk->GetWritable()),
            reinterpret_cast<char *>(pChunk->GetWriter())
        };
        pChunk->pfnIoCallback = X_FwdOnRecv<tChunk>;
        RAII_LOCK(x_mtx);
        if (!x_pIoGroup || x_bStopping)
            throw ExnIllegalState {};
        DWORD dwFlags = 0;
        StartThreadpoolIo(x_pTpIo);
        auto nRes = WSARecv(x_hSocket, &vWsaBuf, 1, nullptr, &dwFlags, pChunk, nullptr);
        ++x_uRecv;
        if (nRes == SOCKET_ERROR) {
            nRes = WSAGetLastError();
            if (nRes != WSA_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndRecv();
                throw ExnSockRead<tChunk> {nRes, pChunk};
            }
        }
    }

    template<class tChunk>
    inline void Write(std::unique_ptr<tChunk> upChunk) {
        WSABUF vWsaBuf {
            static_cast<ULONG>(upChunk->GetReadable()),
            reinterpret_cast<char *>(upChunk->GetReader())
        };
        upChunk->pfnIoCallback = X_FwdOnSend<tChunk>;
        RAII_LOCK(x_mtx);
        if (!x_pIoGroup || x_bStopping)
            throw ExnIllegalState {};
        auto pChunk = upChunk.release();
        StartThreadpoolIo(x_pTpIo);
        auto nRes = WSASend(x_hSocket, &vWsaBuf, 1, nullptr, 0, pChunk, nullptr);
        ++x_uSend;
        if (nRes == SOCKET_ERROR) {
            nRes = WSAGetLastError();
            if (nRes != WSA_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndSend();
                throw ExnSockWrite<tChunk> {nRes, std::unique_ptr<tChunk>(pChunk)};
            }
        }
    }

private:
    void X_Shutdown() noexcept {
        if (x_bStopping)
            return;
        x_bStopping = true;
        shutdown(x_hSocket, SD_BOTH);
        if (!x_uSend)
            X_Close();
    }

    void X_Close() noexcept {
        x_bStopping = true;
        closesocket(x_hSocket);
        if (!x_uRecv)
            X_Finalize();
    }

    inline void X_Finalize() noexcept {
        if (x_pIoGroup) {
            x_pIoGroup->UnregisterIo(x_pTpIo);
            x_pIoGroup = nullptr;
        }
        if (!x_bFinalized) {
            x_bFinalized = true;
            x_vUpper.OnFinalize();
        }
    }

    inline void X_EndRecv() noexcept {
        if (!--x_uRecv && x_bStopping) {
            closesocket(x_hSocket);
            X_Finalize();
        }
    }

    inline void X_EndSend() noexcept {
        if (!--x_uSend && x_bStopping)
            X_Close();
    }

private:
    template<class tChunk>
    inline void X_IocbOnRecv(DWORD dwRes, U32 uDone, tChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnRead(dwRes, 0, pChunk);
        else {
            pChunk->Skip(uDone);
            x_vUpper.OnRead(0, uDone, pChunk);
        }
        RAII_LOCK(x_mtx);
        X_EndRecv();
    }

    template<class tChunk>
    inline void X_IocbOnSend(DWORD dwRes, U32 uDone, tChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnWrite(dwRes, 0, std::unique_ptr<tChunk>(pChunk));
        else {
            pChunk->Discard(uDone);
            x_vUpper.OnWrite(0, uDone, std::unique_ptr<tChunk>(pChunk));
        }
        RAII_LOCK(x_mtx);
        X_EndSend();
    }

    template<class tChunk>
    static void X_FwdOnRecv(
        void *pParam, DWORD dwRes, U32 uDone, ChunkIoContext *pCtx
    ) noexcept {
        reinterpret_cast<SockIo *>(pParam)->X_IocbOnRecv(dwRes, uDone, static_cast<tChunk *>(pCtx));
    }

    template<class tChunk>
    static void X_FwdOnSend(
        void *pParam, DWORD dwRes, U32 uDone, ChunkIoContext *pCtx
    ) noexcept {
        reinterpret_cast<SockIo *>(pParam)->X_IocbOnSend(dwRes, uDone, static_cast<tChunk *>(pCtx));
    }

private:
    Upper &x_vUpper;

    RecursiveMutex x_mtx;
    bool x_bStopping = false;
    bool x_bFinalized = false;

    IoGroup *x_pIoGroup = nullptr;
    PTP_IO x_pTpIo = nullptr;

    U32 x_uSend = 0;
    U32 x_uRecv = 0;
    
    SOCKET x_hSocket;
    SockName x_vSnLocal;
    SockName x_vSnRemote;
    
};
