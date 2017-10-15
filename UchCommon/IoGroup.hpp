#pragma once

#include "Common.hpp"

#include "Sync.hpp"

class IoGroup {
public:
    IoGroup(const IoGroup &) = delete;
    IoGroup(IoGroup &&) = delete;

    IoGroup &operator =(const IoGroup &) = delete;
    IoGroup &operator =(IoGroup &&) = delete;

    inline IoGroup(DWORD dwTickMilli = 10, DWORD dwThreads = GetProcessors() * 2) :
        x_dwThreads(dwThreads), x_dwTickMilli(dwTickMilli)
    {
        InitializeThreadpoolEnvironment(&x_vTpCbEnv);
        x_pTpClGroup = CreateThreadpoolCleanupGroup();
        if (!x_pTpClGroup)
            throw ExnSys();
        SetThreadpoolCallbackCleanupGroup(&x_vTpCbEnv, x_pTpClGroup, nullptr);
    }

    inline ~IoGroup() {
        CloseThreadpoolCleanupGroup(x_pTpClGroup);
    }

public:
    void Start();
    void Shutdown() noexcept;
    PTP_IO RegisterIo(HANDLE hFile, void *pParam);
    void UnregisterIo(PTP_IO &pIo);

    template<class tObj>
    void RegisterTick(tObj &vObj) noexcept {
        RAII_LOCK(x_vTimerCtx.mtx);
        x_vTimerCtx.vecTickCtxs.emplace_back(X_FwdOnTick<tObj>, &vObj);
    }

    template<class tObj>
    void UnregisterTick(tObj &vObj) noexcept {
        RAII_LOCK(x_vTimerCtx.mtx);
        auto it = std::find(x_vTimerCtx.vecTickCtxs.begin(), x_vTimerCtx.vecTickCtxs.end(), &vObj);
        if (it != x_vTimerCtx.vecTickCtxs.end())
            x_vTimerCtx.vecTickCtxs.erase(it);
    }

    template<class tObj>
    void RegisterNetev(tObj &vObj, SOCKET hSocket, WSAEVENT hEvent) {
        if (x_vWaitCtx.pObj)
            throw ExnIllegalState {};
        x_vWaitCtx.pfnOnNetev = X_FwdOnNetev<tObj>;
        x_vWaitCtx.pObj = &vObj;
        x_vWaitCtx.hSocket = hSocket;
        x_vWaitCtx.hEvent = hEvent;
        SetThreadpoolWait(x_vWaitCtx.pTpWait, hEvent, nullptr);
    }

    inline void UnregisterNetev() noexcept {
        if (x_vWaitCtx.pObj) {
            WaitForThreadpoolWaitCallbacks(x_vWaitCtx.pTpWait, TRUE);
            x_vWaitCtx.pObj = nullptr;
        }
    }

    template<class tJob, class ...tvArgs>
    void Post(tJob &&fnJob, tvArgs &&...vArgs) {
        using tUserJob = std::decay_t<decltype(
            std::bind(std::forward<tJob>(fnJob), std::forward<tvArgs>(vArgs)...)
        )>;
        auto pCtx = new X_TpUserContext<tUserJob>(
            std::bind(std::forward<tJob>(fnJob), std::forward<tvArgs>(vArgs)...)
        );
        auto dwbRes = TrySubmitThreadpoolCallback(X_TpcbOnUser<tUserJob>, pCtx, &x_vTpCbEnv);
        if (!dwbRes)
            throw ExnSys();
    }

    inline void Post(PTP_SIMPLE_CALLBACK pTpcbUser, void *pParam) {
        auto dwbRes = TrySubmitThreadpoolCallback(pTpcbUser, pParam, &x_vTpCbEnv);
        if (!dwbRes)
            throw ExnSys();
    }

private:
    TP_CALLBACK_ENVIRON x_vTpCbEnv;
    PTP_CLEANUP_GROUP x_pTpClGroup = nullptr;
    PTP_POOL x_pTpPool = nullptr;
    DWORD x_dwThreads;
    DWORD x_dwTickMilli;

private:
    using IocbTickHandler = bool(void *pObj, U64 usNow);

    template<class tObj>
    static bool X_FwdOnTick(void *pObj, U64 uStampNow) noexcept {
        return reinterpret_cast<tObj *>(pObj)->OnTick(uStampNow);
    }

    struct X_TickContext {
        constexpr X_TickContext(IocbTickHandler *pfnOnTick_, void *pParam_) noexcept :
            pfnOnTick(pfnOnTick_), pObj(pParam_) {}
        constexpr bool operator ==(void *pParam_) noexcept {
            return pObj == pParam_;
        }

        IocbTickHandler *pfnOnTick;
        void *pObj;

    };

    struct X_TpTimerContext {
        PTP_TIMER pTpTimer = nullptr;
        RecursiveMutex mtx;
        std::vector<X_TickContext> vecTickCtxs;
    } x_vTimerCtx {};

private:
    using IocbNetevHandler = bool(void *pObj, WSANETWORKEVENTS &vNetev);

    template<class tObj>
    static bool X_FwdOnNetev(void *pObj, WSANETWORKEVENTS &vNetev) noexcept {
        return reinterpret_cast<tObj *>(pObj)->OnNetev(vNetev);
    }

    struct X_TpWaitContext {
        PTP_WAIT pTpWait = nullptr;
        IocbNetevHandler *pfnOnNetev;
        void *pObj;
        SOCKET hSocket;
        WSAEVENT hEvent;
    } x_vWaitCtx {};
    
private:
    template<class tUserJob>
    struct X_TpUserContext {
        template<class tArg>
        inline X_TpUserContext(tArg &&vArg) noexcept : vUserJob(std::forward<tArg>(vArg)) {}

        tUserJob vUserJob;

    };

private:    
    static VOID CALLBACK X_TpcbOnIo(
        PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam,
        PVOID pOverlapped, ULONG uRes, ULONG_PTR uDone, PTP_IO pTpIo
    ) noexcept;

    static VOID CALLBACK X_TpcbOnTimer(
        PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam, PTP_TIMER pTpTimer
    ) noexcept;

    static VOID CALLBACK X_TpcbOnWait(
        PTP_CALLBACK_INSTANCE pTpcbInst, PVOID pParam,
        PTP_WAIT pTpWait, TP_WAIT_RESULT dwRes
    ) noexcept;

    template<class tUserJob>
    static VOID CALLBACK X_TpcbOnUser(PTP_CALLBACK_INSTANCE pCbInst, PVOID pParam) noexcept {
        UNREFERENCED_PARAMETER(pCbInst);
        auto pCtx = reinterpret_cast<X_TpUserContext<tUserJob> *>(pParam);
        pCtx->vUserJob();
        delete pCtx;
    }

};
