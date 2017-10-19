#include "Common.hpp"

#include "ByteChunk.hpp"
#include "IoGroup.hpp"

#include "Debug.hpp"

void IoGroup::Start() {
    if (x_pTpPool)
        throw ExnIllegalState {};
    x_pTpPool = CreateThreadpool(nullptr);
    if (!x_pTpPool)
        throw ExnSys();
    SetThreadpoolThreadMaximum(x_pTpPool, x_dwThreads);
    SetThreadpoolThreadMinimum(x_pTpPool, x_dwThreads);
    SetThreadpoolCallbackPool(&x_vTpCbEnv, x_pTpPool);
    if (x_dwTickMilli) {
        x_vTimerCtx.pTpTimer = CreateThreadpoolTimer(X_TpcbOnTimer, &x_vTimerCtx, &x_vTpCbEnv);
        if (!x_vTimerCtx.pTpTimer) {
            auto dwError = GetLastError();
            CloseThreadpool(x_pTpPool);
            x_pTpPool = nullptr;
            throw ExnSys(dwError);
        }
        FILETIME vFileTime {};
        SetThreadpoolTimer(x_vTimerCtx.pTpTimer, &vFileTime, x_dwTickMilli, 0);
    }
    x_vWaitCtx.pTpWait = CreateThreadpoolWait(X_TpcbOnWait, &x_vWaitCtx, &x_vTpCbEnv);
    if (!x_vWaitCtx.pTpWait) {
        auto dwError = GetLastError();
        CloseThreadpoolCleanupGroupMembers(x_pTpClGroup, TRUE, nullptr);
        CloseThreadpool(x_pTpPool);
        x_pTpPool = nullptr;
        throw ExnSys(dwError);
    }
}

void IoGroup::Shutdown() noexcept {
    CloseThreadpoolCleanupGroupMembers(x_pTpClGroup, TRUE, nullptr);
    if (x_pTpPool) {
        CloseThreadpool(x_pTpPool);
        x_pTpPool = nullptr;
    }
}

PTP_IO IoGroup::RegisterIo(HANDLE hFile, void *pParam) {
    auto pTpIo = CreateThreadpoolIo(hFile, X_TpcbOnIo, pParam, &x_vTpCbEnv);
    if (!pTpIo)
        throw ExnSys();
    return pTpIo;
}

void IoGroup::UnregisterIo(PTP_IO &pTpIo) {
    if (pTpIo) {
        CloseThreadpoolIo(pTpIo);
        pTpIo = nullptr;
    }
}

VOID IoGroup::X_TpcbOnIo(
    PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam,
    PVOID pOverlapped, ULONG uRes, ULONG_PTR uDone, PTP_IO pTpIo
) noexcept {
    UNREFERENCED_PARAMETER(pTpcbInst);
    UNREFERENCED_PARAMETER(pTpIo);
    auto pCtx = static_cast<ChunkIoContext *>(
        reinterpret_cast<OVERLAPPED *>(pOverlapped)
    );
    auto pfnIoCallback = pCtx->pfnIoCallback;
    pfnIoCallback(pParam, static_cast<DWORD>(uRes), static_cast<U32>(uDone), pCtx);
}

VOID IoGroup::X_TpcbOnTimer(
    PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam, PTP_TIMER pTpTimer
) noexcept {
    UNREFERENCED_PARAMETER(pTpcbInst);
    UNREFERENCED_PARAMETER(pTpTimer);
    auto pCtx = reinterpret_cast<X_TpTimerContext *>(pParam);
    auto usNow = GetTimeStamp();
    if (!pCtx->mtx.TryAcquire()) { // lag
        DBG_PRINTLN("lag");
        return;
    }
    RAII_LOCK_ACQUIRED(pCtx->mtx);
    for (auto it = pCtx->liTickCtxs.begin(); it != pCtx->liTickCtxs.end(); ) {
        if (it->pfnOnTick(it->pObj, usNow))
            ++it;
        else
            it = pCtx->liTickCtxs.erase(it);
    }
}

VOID IoGroup::X_TpcbOnWait(
    PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam,
    PTP_WAIT pTpWait, TP_WAIT_RESULT dwRes
) noexcept {
    UNREFERENCED_PARAMETER(pTpcbInst);
    UNREFERENCED_PARAMETER(pTpWait);
    UNREFERENCED_PARAMETER(dwRes);
    assert(dwRes == WAIT_OBJECT_0);
    auto pCtx = reinterpret_cast<X_TpWaitContext *>(pParam);
    WSANETWORKEVENTS vNetev;
    WSAEnumNetworkEvents(pCtx->hSocket, pCtx->hEvent, &vNetev);
    if (pCtx->pfnOnNetev(pCtx->pObj, vNetev))
        SetThreadpoolWait(pCtx->pTpWait, pCtx->hEvent, nullptr);
}
