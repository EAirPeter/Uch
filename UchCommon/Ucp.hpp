#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IntrList.hpp"
#include "SockIo.hpp"
#include "Sync.hpp"
#include "UcpSeg.hpp"

#include "Debug.hpp"

#define UCP_TRANSMIT

namespace ImplUcp {

    constexpr U32 FloorPow2(U32 uVal) noexcept {
        while (uVal & (uVal - 1))
            uVal -= uVal & (0 - uVal);
        return uVal;
    }

#if 0
#   define UCP_DBGOUT(...) DBG_PRINTLN( \
        L"[Ucp ", std::setw(4), std::setfill(L'0'), std::hex, std::uppercase, ((UPtr) this & 0xffff), \
        L"][", std::setw(18), std::setfill(L' '), __func__, "] ", std::dec, __VA_ARGS__ \
    )
#else
#   define UCP_DBGOUT(...) ((void) 0)
#endif

    template<class tUpper>
    class Ucp {
    private:
        using Upper = tUpper;
        using Lower = SockIo<Ucp>;

        using UpSeg = SegPool::UniquePtr;

    public:
        Ucp(const Ucp &) = delete;
        Ucp(Ucp &&) = delete;

        Ucp(Upper &vUpper, SOCKET hSocket) : x_vUpper(vUpper), x_vLower(*this, hSocket) {}

        ~Ucp() {
            if (x_pIoGroup)
                x_pIoGroup->UnregisterTick(*this);
        }

        Ucp &operator =(const Ucp &) = delete;
        Ucp &operator =(Ucp &&) = delete;

    public:
        constexpr Upper &GetUpper() noexcept {
            return x_vUpper;
        }

        constexpr Lower &GetLower() noexcept {
            return x_vLower;
        }

        constexpr SegPool &GetSegPool() const noexcept {
            return x_vPool;
        }

    public:
        inline U32 GetQueueSize() noexcept {
            return x_atmuzQue.load();
        }

        inline U64 GetReceivedSize() noexcept {
            return x_atmuzRcvd.load();
        }

        inline U64 GetSentSize() noexcept {
            return x_atmuzSent.load();
        }

        inline void EndOnPacket(U32 uzSize) noexcept {
            x_atmuzAsm.fetch_add(uzSize);
        }

        inline UcpBuffer MakeBuffer() const noexcept {
            return {x_vPool};
        }

    public:
        inline void AssignToIoGroup(IoGroup &vIoGroup) {
            RAII_LOCK(x_mtx);
            x_vLower.AssignToIoGroup(vIoGroup);
            assert(!x_pIoGroup);
            vIoGroup.RegisterTick(*this);
            x_pIoGroup = &vIoGroup;
            for (DWORD i = 0; i < vIoGroup.GetThreads(); ++i)
                X_PostRead(x_vPool.MakeUnique(UcpSeg::SegRecv {}));
        }

        inline void Shutdown() noexcept {
            RAII_LOCK(x_mtx);
            x_vState += x_kubStopping;
            if (!x_vState(x_kubNeedAck) && x_qQue.IsEmpty() && x_qSnd.IsEmpty())
                x_vLower.Shutdown();
        }

        inline void Close() noexcept {
            RAII_LOCK(x_mtx);
            x_vState += x_kubStopping;
            if (x_pIoGroup) {
                x_pIoGroup->UnregisterTick(*this);
                x_pIoGroup = nullptr;
            }
            x_vLower.Close();
        }

        void OnFinalize() noexcept {
            RAII_LOCK(x_mtx);
            x_vState += x_kubStopping;
            x_vUpper.OnFinalize();
        }

        inline void OnRead(DWORD dwError, U32 uDone, UcpSeg *pSeg) {
            auto upSeg = x_vPool.Wrap(pSeg);
            if (dwError) {
                // just let the RDT handle this
                ImplDbg::Println("read fail");
                {
                    RAII_LOCK(x_mtx);
                    if (x_vState(x_kubConnLost))
                        return;
                }
                X_PostRead(std::move(upSeg));
                return;
            }
            assert(uDone);
            assert(uDone == pSeg->GetReadable());
            pSeg->Decode();
            auto upNewSeg = x_vPool.MakeUnique(UcpSeg::SegRecv {});
            U32 uzRwndAlloc;
            SegQue qAsm(x_vPool);
            {
                RAII_LOCK(x_mtx);
                if (x_vState(x_kubConnLost))
                    return;
                X_PostRead(std::move(upNewSeg));
                uzRwndAlloc = X_InputSegment(qAsm, std::move(upSeg));
            }
            x_atmuzAsm.fetch_sub(uzRwndAlloc);
            while (!qAsm.IsEmpty()) {
                auto vBuf = MakeBuffer();
                auto uzRwndAlloc = qAsm.GetHead()->GetSize();
                vBuf.PushChunk(qAsm.PopHead());
                while (vBuf.GetTail()->vFlags(kubSegFrg)) {
                    uzRwndAlloc += qAsm.GetHead()->GetSize();
                    vBuf.PushChunk(qAsm.PopHead());
                }
                x_vUpper.OnPacket(uzRwndAlloc, vBuf);
            }
        }

#ifndef UCP_TRANSMIT
        inline void OnWrite(DWORD dwError, U32 uDone, UcpSeg *pSeg) {
            UNREFERENCED_PARAMETER(dwError);
            UNREFERENCED_PARAMETER(uDone);
            UNREFERENCED_PARAMETER(pSeg);
            // just let the RDT handle this
            if (!pSeg->vFlags(kubSegPsh))
                x_vPool.Delete(pSeg);
        }
#else
        inline void OnTransmit(DWORD dwError, U32 uDone, ChunkIoContext *pCtx) {
            UNREFERENCED_PARAMETER(dwError);
            UNREFERENCED_PARAMETER(uDone);
            UNREFERENCED_PARAMETER(pCtx);
            x_vecTmpSegs.clear();
            x_vecTpes.clear();
            x_atmbTsm.clear();
        }
#endif

        bool OnTick(U64 usNow) noexcept {
#ifdef UCP_TRANSMIT
            bool bTsmAcq = false;
#endif
            {
                RAII_LOCK(x_mtx);
                if (x_vState(x_kubStopping) && x_qQue.IsEmpty() && x_qSnd.IsEmpty()) {
                    x_vLower.Shutdown();
                    return false;
                }
                x_usNow = usNow;
                constexpr auto ubFlags = x_kubNeedAck | x_kubNeedAsk | x_kubDirty;
                if (x_vState(ubFlags) || StampDue(x_usNow, x_usTimeout)) {
                    try {
#ifdef UCP_TRANSMIT
                        bTsmAcq =
#endif
                        X_Flush();
                    }
                    catch (ExnIllegalState) {
                        // connection lost
                        x_vState += x_kubConnLost | x_kubStopping;
                        x_vLower.Close();
                        x_vUpper.OnForciblyClose();
                        return false;
                    }
                }
                x_atmuzQue.store(x_qQue.GetSize());
                x_atmuzRcvd.store(x_uzRcvd);
                x_atmuzSent.store(x_uzSent);
            }
#ifdef UCP_TRANSMIT
            if (bTsmAcq) {
                if (!x_vecTpes.empty())
                    X_PostTransmit();
                else
                    x_atmbTsm.clear();
            }
#endif
            return true;
        }

    public:
        inline void PostBuffer(UcpBuffer &qPak) {
            for (auto pSeg = qPak.GetHead(); pSeg != qPak.GetNil(); pSeg = pSeg->GetNext()) {
                pSeg->uzData = pSeg->GetReadable();
                pSeg->vFlags = kubSegPsh | kubSegFrg;
            }
            qPak.GetTail()->vFlags = kubSegPsh;
            RAII_LOCK(x_mtx);
            if (x_vState(x_kubStopping))
                throw ExnIllegalState {};
            x_qQue.Splice(qPak);
            x_vState += x_kubDirty;
        }

        inline void PostBuffer(UcpBuffer &&vBuf) {
            PostPacket(vBuf);
        }
        template<class tPacket>
        inline void PostPacket(tPacket &&vPacket) {
            PostBuffer(MakeBuffer() << vPacket);
        }

    private:
        inline U32 X_InputSegment(SegQue &qAsm, UpSeg upRcv) noexcept {
            x_vState += x_kubEchoed;
            if (!upRcv->ucRwnd)
                x_vState += x_kubNeedAsk;
            else {
                x_vState -= x_kubNeedAsk | x_kubAsking;
                x_ucAskTimedOut = 0;
                if (!x_ucRmtRwnd)
                    x_vState += x_kubDirty;
            }
            x_ucRmtRwnd = upRcv->ucRwnd;
            if (upRcv->vFlags(kubSegAsk))
                x_vState += x_kubNeedAck;
            U32 uzAcked = 0;
            auto pSnd = x_qSnd.GetHead();
            while (pSnd != x_qSnd.GetNil() && SeqBefore(pSnd->unSeq, upRcv->unAck)) {
                assert(pSnd->ucSent);
                X_UpdateRtt(pSnd);
                uzAcked += pSnd->GetSize();
                x_qSnd.Remove(pSnd);
                pSnd = x_qSnd.GetHead();
            }
            if (upRcv->vFlags(kubSegSak)) {
                U64 usSakLatest = StampInfinite(x_usNow);
                auto pSak = upRcv->GetReader();
                for (U32 i = 0; i != upRcv->uzData; ++i) {
                    U32 unSak = *reinterpret_cast<U32 *>(pSak) & 0x00ffffff;
                    while (pSnd != x_qSnd.GetNil() && SeqBefore(pSnd->unSeq, unSak))
                        pSnd = pSnd->GetNext();
                    if (pSnd != x_qSnd.GetNil() && pSnd->unSeq == unSak) {
                        assert(pSnd->ucSent);
                        if (StampBefore(usSakLatest, pSnd->usSent))
                            usSakLatest = pSnd->usSent;
                        X_UpdateRtt(pSnd);
                        uzAcked += pSnd->GetSize();
                        x_qSnd.Remove(pSnd);
                        pSnd = x_qSnd.GetHead();
                    }
                    pSak += 3;
                }
                for (pSnd = x_qSnd.GetHead(); pSnd != x_qSnd.GetNil() && pSnd->ucSent; pSnd = pSnd->GetNext())
                    if (StampBefore(pSnd->usSent, usSakLatest))
                        ++pSnd->ucSkipped;
            }
            x_uzFlight -= uzAcked;
            x_uzSent += uzAcked;
            if (uzAcked) {
                x_vState += x_kubDirty;
                x_unSndAck = x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq;
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, kuzMss);
                else
                    x_uzCwnd += std::min(kuzMss * kuzMss / x_uzCwnd, kuzMss);
            }
            if(upRcv->vFlags(kubSegPsh)) {
                do {
                    if (!SeqBefore(upRcv->unSeq, x_unRcvSeq + x_kucBuf))
                        break;
                    x_vState += x_kubNeedAck;
                    if (SeqBefore(upRcv->unSeq, x_unRcvSeq))
                        break;
                    x_vecSndSaks.emplace_back(upRcv->unSeq);
                    auto &upQue = x_aqRcv[upRcv->unSeq % x_kucBuf];
                    if (upQue)
                        break;
                    x_uzRcvd += upRcv->GetSize();
                    upQue = std::move(upRcv);
                    U32 uzRwndAlloc = 0;
                    auto unBound = SeqIncrease(x_unRcvSeq, x_kucBuf);
                    while (x_unRcvSeq != unBound && x_aqRcv[x_unRcvSeq % x_kucBuf]) {
                        x_qAsm.PushTail(std::move(x_aqRcv[x_unRcvSeq % x_kucBuf]));
                        x_unRcvSeq = SeqIncrease(x_unRcvSeq);
                        auto pAsm = x_qAsm.GetTail();
                        uzRwndAlloc += pAsm->GetSize();
                        if (!pAsm->vFlags(kubSegFrg))
                            qAsm.Splice(x_qAsm);
                    }
                    return uzRwndAlloc;
                }
                while (false);
            }
            return 0;
        }

        inline void X_UpdateRtt(UcpSeg *pSeg) noexcept {
            if (pSeg->ucSent != 1) {
                UCP_DBGOUT("ignored");
                return;
            }
            auto utRtt = x_usNow - pSeg->usSent;
            assert(static_cast<I64>(utRtt) >= 0);
            // update x_utSRtt, x_utRttVar, x_utRto according to RFC6298
            if (!x_utSRtt) {
                x_utSRtt = utRtt;
                x_utRttVar = utRtt >> 1;
            }
            else {
                auto uTmp = utRtt > x_utSRtt ? utRtt - x_utSRtt : x_utSRtt - utRtt;
                x_utRttVar = (x_utRttVar * 3 + uTmp) >> 2;
                x_utSRtt = (x_utSRtt * 7 + utRtt) >> 3;
            }
            x_utRto = std::max(x_utSRtt + std::max(x_kutTick, x_utRttVar << 2), x_kutRtoMin);
            UCP_DBGOUT("srtt = ", x_utSRtt, ", rttvar = ", x_utRttVar, ", rto = ", x_utRto);
        }

#ifdef UCP_TRANSMIT
        inline bool X_Flush() {
            UCP_DBGOUT("");
            X_PrepareSaks();
            X_PrepareQSnd();
            if (x_atmbTsm.test_and_set()) {
                // lag
                x_vState += x_kubDirty;
                return false;
            }
#else
        inline void X_Flush() {
            UCP_DBGOUT("");
            X_PrepareSaks();
            X_PrepareQSnd();
#endif
            x_vState -= x_kubDirty | x_kubEchoed;
            if (x_vState(x_kubAsking)) {
                if (StampDue(x_usNow, x_usAskTimeout)) {
                    // ImplDbg::Println("ask timeout");
                    if (++x_ucAskTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
                    x_vState += x_kubNeedAsk;
                    x_utAskRto = std::min(x_kutRtoMax, x_utAskRto + (x_utAskRto >> 1));
                }
                else
                    x_vState -= x_kubNeedAsk;
            }
            else
                x_utAskRto = x_utRto;
            if (x_vState(x_kubNeedAsk)) {
                x_vState += x_kubAsking;
                x_usAskTimeout = x_usNow + x_utAskRto;
            }
            if (x_vState(x_kubAsking))
                x_usTimeout = x_usAskTimeout;
            else
                x_usTimeout = StampInfinite(x_usNow);
            auto uzRwnd = x_atmuzAsm.load();
            auto ucRwnd = uzRwnd & 0x80000000U ? 0 : uzRwnd / kuzMss;
            auto ucSaks = static_cast<U32>(x_vecSndSaks.size());
            auto ucSakSegs = (ucSaks + kuMaxSaks - 1) / kuMaxSaks;
            auto uzSakSegs = ucSakSegs * kuzUcpHdr + ucSaks * 3;
            auto uzCwnd = x_uzCwnd > uzSakSegs ? x_uzCwnd - uzSakSegs : 0;
            auto uzPshSegs = std::min(uzCwnd, x_ucRmtRwnd * kuzMss);

            bool bFastResend = false;
            bool bTimedOut = false;
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (pSeg->GetSize() > uzPshSegs) {
                    if (StampBefore(pSeg->usTimeout, x_usTimeout))
                        x_usTimeout = pSeg->usTimeout;
                    continue;
                }
                if (!pSeg->ucSent) {
                    // first time
                    x_uzFlight += pSeg->GetSize();
                    uzPshSegs -= pSeg->GetSize();
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    bTimedOut = true;
                    if (x_vState(x_kubEchoed))
                        pSeg->ucTimedOut = 0;
                    else if (++pSeg->ucTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
                    uzPshSegs -= pSeg->GetSize();
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
                    bFastResend = true;
                    uzPshSegs -= pSeg->GetSize();
                    X_FlushPayload(pSeg, ucRwnd);
                }
                if (StampBefore(pSeg->usTimeout, x_usTimeout))
                    x_usTimeout = pSeg->usTimeout;
            }
            X_FlushSaks(ucRwnd);
            // update cwnd and ssthresh according to RFC5681
            if (bTimedOut) {
                // ImplDbg::Println("timeout");
                x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd >> 1);
                x_uzCwnd = x_kuzCwndMin;
            }
            else if (bFastResend) {
                // ImplDbg::Println("resend");
                x_uzSsthresh = std::max(x_kuzSsthreshMin, std::max(x_uzCwnd, x_uzFlight) >> 1);
                x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuzMss;
            }
            UCP_DBGOUT(
                "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                ", rwnd = ", x_atmuzAsm.load(), ", xrwnd = ", x_ucRmtRwnd,
                ", now = ", x_usNow, ", next_timeout = ", x_usTimeout
            );
#ifdef UCP_TRANSMIT
            return true;
#endif
        }

        inline void X_PrepareSaks() noexcept {
            U32 ucSaks = 0;
            for (auto unSak : x_vecSndSaks)
                if (SeqBefore(x_unRcvSeq, unSak))
                    x_vecSndSaks[ucSaks++] = unSak;
            x_vecSndSaks.resize(ucSaks);
            std::sort(x_vecSndSaks.begin(), x_vecSndSaks.end(), std::greater<U32> {});
            auto itEnd = std::unique(x_vecSndSaks.begin(), x_vecSndSaks.end());
            x_vecSndSaks.resize(static_cast<USize>(itEnd - x_vecSndSaks.begin()));
            UCP_DBGOUT("saks = ", x_vecSndSaks);
        }

        inline void X_PrepareQSnd() noexcept {
            auto unBound = SeqIncrease(x_unSndAck, x_kucBuf);
            while (!x_qQue.IsEmpty() && SeqBefore(x_unSndSeq, unBound)) {
                auto upSeg = x_qQue.PopChunk();
                upSeg->unSeq = std::exchange(x_unSndSeq, SeqIncrease(x_unSndSeq));
                upSeg->ucSent = 0;
                upSeg->ucTimedOut = 0;
                x_qSnd.PushTail(std::move(upSeg));
            }
            UCP_DBGOUT("flight = ", x_uzFlight);
        }

        inline void X_FlushPayload(UcpSeg *pSeg, U32 ucRwnd) noexcept {
            x_vState -= x_kubNeedAck | x_kubNeedAsk;
            pSeg->unAck = x_unRcvSeq;
            pSeg->ucRwnd = ucRwnd;
            ++pSeg->ucSent;
            pSeg->ucSkipped = 0;
            pSeg->usSent = x_usNow;
            pSeg->usTimeout = x_usNow + x_utRto;
            UCP_DBGOUT("seq = ", pSeg->unSeq, ", ack = ", pSeg->unAck,
                ", timedout = ", pSeg->ucTimedOut, ", timeout = ", pSeg->usTimeout
            );
            pSeg->Encode();
#ifndef UCP_TRANSMIT
            X_PostWrite(pSeg);
#else
            X_PushTpe(pSeg);
#endif
        }

        inline void X_FlushSaks(U32 ucRwnd) noexcept {
            if (!x_vecSndSaks.empty()) {
                x_vState -= x_kubNeedAck;
                while (!x_vecSndSaks.empty()) {
                    auto uSaksToSend = std::min((kuzMss - kuzUcpHdr) / 3, static_cast<U32>(x_vecSndSaks.size()));
                    auto ubFlags = kubSegSak | (x_vState(x_kubNeedAsk) ? kubSegAsk : 0);
                    x_vState -= x_kubNeedAsk;
                    auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, uSaksToSend, ubFlags);
                    UCP_DBGOUT("saks = ", uSaksToSend);
                    while (uSaksToSend--) {
                        upSeg->Write(&x_vecSndSaks.back(), 3);
                        x_vecSndSaks.pop_back();
                    }
#ifndef UCP_TRANSMIT
                    X_PostWrite(std::move(upSeg));
#else
                    X_PushTpe(upSeg.get());
                    x_vecTmpSegs.emplace_back(std::move(upSeg));
#endif
                }
            }
            else if (x_vState(x_kubNeedAck | x_kubNeedAsk)) {
                auto ubFlags = x_vState(x_kubNeedAsk) ? kubSegAsk : 0;
                x_vState -= x_kubNeedAck | x_kubNeedAsk;
                auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, 0, ubFlags);
                UCP_DBGOUT("ack = ", x_unRcvSeq);
#ifndef UCP_TRANSMIT
                X_PostWrite(std::move(upSeg));
#else
                X_PushTpe(upSeg.get());
                x_vecTmpSegs.emplace_back(std::move(upSeg));
#endif
            }
        }

#ifdef UCP_TRANSMIT
        inline void X_PushTpe(UcpSeg *pSeg) noexcept {
            x_vecTpes.emplace_back();
            x_vecTpes.back().dwElFlags = TP_ELEMENT_MEMORY | TP_ELEMENT_EOP;
            x_vecTpes.back().cLength = static_cast<ULONG>(pSeg->GetReadable());
            x_vecTpes.back().pBuffer = pSeg->GetReader();
        }
#endif

    private:
        inline void X_PostRead(UpSeg upSeg) noexcept {
            upSeg->ToBegin();
            auto pSeg = upSeg.release();
            try {
                x_vLower.PostRead(pSeg);
            }
            catch (ExnSockIo<UcpSeg> &) {
                // just let the RDT handle this
                x_vPool.Delete(pSeg);
            }
            catch (ExnIllegalState) {
                // active shutdown
                x_vPool.Delete(pSeg);
            }
        }

#ifndef UCP_TRANSMIT
        inline void X_PostWrite(UcpSeg *pSeg) noexcept {
            assert(pSeg->vFlags(kubSegPsh));
            try {
                x_vLower.Write<UcpSeg>(pSeg);
            }
            catch (ExnSockIo<UcpSeg> &) {
                // just let the RDT handle this
            }
            catch (ExnIllegalState) {
                // just let the RDT handle this
            }
        }

        inline void X_PostWrite(UpSeg upSeg) noexcept {
            assert(!upSeg->vFlags(kubSegPsh));
            auto pSeg = upSeg.release();
            try {
                x_vLower.Write<UcpSeg>(pSeg);
            }
            catch (ExnSockIo<UcpSeg> &) {
                // just let the RDT handle this
                x_vPool.Delete(pSeg);
            }
            catch (ExnIllegalState) {
                // just let the RDT handle this
                x_vPool.Delete(pSeg);
            }
        }

#else
        inline void X_PostTransmit() noexcept {
            try {
                x_vLower.Transmit(x_vecTpes.data(), static_cast<U32>(x_vecTpes.size()), kuzMss, &x_vTsmCtx);
            }
            catch (...) {
                OnTransmit(0, 0, nullptr);
            }
        }
#endif

    private:
        mutable SegPool x_vPool;

        Upper &x_vUpper;
        Lower x_vLower;

        Mutex x_mtx;
        IoGroup *x_pIoGroup = nullptr;

#ifdef UCP_TRANSMIT
        ChunkIoContext x_vTsmCtx {};
        std::vector<TRANSMIT_PACKETS_ELEMENT> x_vecTpes;
        std::vector<UpSeg> x_vecTmpSegs;
        std::atomic_flag x_atmbTsm {};
#endif

    private:
        constexpr static U64 x_kutRtoMin = 400'000;
        constexpr static U64 x_kutRtoMax = 4'000'000;
        constexpr static U64 x_kutTick = 10'000;
        constexpr static U32 x_kucFastResend = 2;
        constexpr static U32 x_kucConnLost = 5;
        constexpr static U32 x_kuzSsthreshInit = 256 << 20;
        constexpr static U32 x_kuzSsthreshMin = 2 * kuzMss;
        constexpr static U32 x_kuzCwndMin = 1 * kuzMss;
        constexpr static U32 x_kuzCwndInit = 3 * kuzMss;
        constexpr static U32 x_kuzAsmMax = 256 << 20;
        constexpr static U32 x_kucBuf = FloorPow2((16 << 20) / kuzMss);

        constexpr static U32 x_kubAsking = 0x00000001;   // ask pending
        constexpr static U32 x_kubConnLost = 0x00000002; // timedout exceeded
        constexpr static U32 x_kubDirty = 0x00000004;    // flush needed
        constexpr static U32 x_kubEchoed = 0x00000008;   // anything received
        constexpr static U32 x_kubNeedAck = 0x00000010;  // payload received, ask received
        constexpr static U32 x_kubNeedAsk = 0x00000020;  // rmt-rwnd = 0
        constexpr static U32 x_kubStopping = 0x80000040; // shutting down

    private:
        Flags<U32> x_vState = 0;
        U64 x_usNow = 0;         // last tick time
        U64 x_usTimeout = 0;     // next time for resend
        U64 x_usAskTimeout = 0;
        U64 x_utAskRto = 0;
        U32 x_ucAskTimedOut = 0;

        U64 x_utRttVar = 0;        // RTTVAR in RFC6298
        U64 x_utSRtt = 0;          // SRTT in RFC6298
        U64 x_utRto = x_kutRtoMax; // RTO in RFC6298

        U32 x_uzSsthresh = x_kuzSsthreshInit;  // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;          // cwnd in RFC5681
        U32 x_uzFlight = 0;                    // FlightSize in RFC 5681
        U32 x_ucRmtRwnd = x_kuzAsmMax / kuzMss; // rwnd of the other side, in mss-s

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        UcpBuffer x_qQue {x_vPool}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {x_vPool};    // segments sent waiting for acknowledgement
        UpSeg x_aqRcv[x_kucBuf] {}; // segments received, may not be correctly ordered
        SegQue x_qAsm {x_vPool};    // segments received and ordered, waiting to be reassemble to packets

        U64 x_uzRcvd = 0; // bytes received
        U64 x_uzSent = 0; // bytes sent

        std::atomic<U32> x_atmuzAsm = x_kuzAsmMax;   // available payload size in x_qAsm and qAsm-s
        std::atomic<U32> x_atmuzQue = 0;
        std::atomic<U64> x_atmuzRcvd = 0;
        std::atomic<U64> x_atmuzSent = 0;

        std::vector<U32> x_vecSndSaks; // saks to be sent

    };

}

using ImplUcp::Ucp;
