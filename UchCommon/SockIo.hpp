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
    tChunk *pChunk;
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
        if (x_atmuState.fetch_or(x_kubAssigned) & x_kubAssigned)
            throw ExnIllegalState();
        x_pTpIo = vIoGroup.RegisterIo(reinterpret_cast<HANDLE>(x_hSocket), this);
        x_pIoGroup = &vIoGroup;
    }

    // does not cancel pending send requests
    inline void Shutdown() noexcept {
        if (x_atmuState.fetch_or(x_kubStopping) & x_kubStopping)
            return;
        shutdown(x_hSocket, SD_BOTH);
        if (!(x_atmuState.load() & x_kumSend)) {
            closesocket(x_hSocket);
            if (!(x_atmuState.load() & x_kumRecv))
                X_Finalize();
        }
    }

    // cancels all pending requests
    inline void Close() noexcept {
        X_Close();
    }

    template<class tChunk>
    inline void PostRead(tChunk *pChunk) {
        WSABUF vWsaBuf {
            static_cast<ULONG>(pChunk->GetWritable()),
            reinterpret_cast<char *>(pChunk->GetWriter())
        };
        pChunk->pfnIoCallback = X_FwdOnRecv<tChunk>;
        DWORD dwFlags = 0;
        StartThreadpoolIo(x_pTpIo);
        auto uState = x_atmuState.fetch_add(x_kucRecv);
        if (!(uState & x_kubAssigned) || (uState & x_kubStopping)) {
            X_EndRecv();
            throw ExnIllegalState {};
        }
        auto nRes = WSARecv(x_hSocket, &vWsaBuf, 1, nullptr, &dwFlags, pChunk, nullptr);
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
    inline void Write(tChunk *pChunk) {
        WSABUF vWsaBuf {
            static_cast<ULONG>(pChunk->GetReadable()),
            reinterpret_cast<char *>(pChunk->GetReader())
        };
        pChunk->pfnIoCallback = X_FwdOnSend<tChunk>;
        StartThreadpoolIo(x_pTpIo);
        auto uState = x_atmuState.fetch_add(x_kucSend);
        if (!(uState & x_kubAssigned) || (uState & x_kubStopping)) {
            X_EndSend();
            throw ExnIllegalState {};
        }
        auto nRes = WSASend(x_hSocket, &vWsaBuf, 1, nullptr, 0, pChunk, nullptr);
        if (nRes == SOCKET_ERROR) {
            nRes = WSAGetLastError();
            if (nRes != WSA_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndSend();
                throw ExnSockWrite<tChunk> {nRes, pChunk};
            }
        }
    }

private:
    void X_Close() noexcept {
        x_atmuState.fetch_or(x_kubStopping);
        X_CloseSocket();
        if (!(x_atmuState.load() & x_kumRecv))
            X_Finalize();
    }

    inline void X_Finalize() noexcept {
        if (x_atmuState.fetch_and(~x_kubAssigned) & x_kubAssigned)
            x_pIoGroup->UnregisterIo(x_pTpIo);
        if (!(x_atmuState.fetch_or(x_kubFinalized) & x_kubFinalized))
            x_vUpper.OnFinalize();
    }

    inline void X_EndRecv() noexcept {
        auto uState = x_atmuState.fetch_sub(x_kucRecv);
        if ((uState & x_kubStopping) && (uState & x_kumRecv) == x_kucRecv) {
            X_CloseSocket();
            X_Finalize();
        }
    }

    inline void X_EndSend() noexcept {
        auto uState = x_atmuState.fetch_sub(x_kucSend);
        if ((uState & x_kubStopping) && (uState & x_kumSend) == x_kucSend)
            X_Close();
    }

    inline void X_CloseSocket() noexcept {
        if (!(x_atmuState.fetch_or(x_kubClosed) & x_kubClosed))
            closesocket(x_hSocket);
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
        X_EndRecv();
    }

    template<class tChunk>
    inline void X_IocbOnSend(DWORD dwRes, U32 uDone, tChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnWrite(dwRes, 0, pChunk);
        else {
            pChunk->Discard(uDone);
            x_vUpper.OnWrite(0, uDone, pChunk);
        }
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
    constexpr static U64 x_kubStopping = 0x8000000000000000;
    constexpr static U64 x_kubAssigned = 0x4000000000000000;
    constexpr static U64 x_kubFinalized = 0x2000000000000000;
    constexpr static U64 x_kubClosed = 0x1000000000000000;
    constexpr static U64 x_kumRecv = 0x0fffffffc0000000;
    constexpr static U64 x_kumSend = 0x000000003fffffff;
    constexpr static U64 x_kucRecv = 0x0000000040000000;
    constexpr static U64 x_kucSend = 0x0000000000000001;

private:
    Upper &x_vUpper;

    std::atomic<U64> x_atmuState = 0;

    IoGroup *x_pIoGroup = nullptr;
    PTP_IO x_pTpIo = nullptr;

    SOCKET x_hSocket;
    SockName x_vSnLocal;
    SockName x_vSnRemote;
    
};
