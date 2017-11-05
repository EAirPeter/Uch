#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IntrList.hpp"
#include "SockIo.hpp"
#include "Sync.hpp"
#include "UcpSeg.hpp"

#define UCP_TRANSMIT
// #define UCP_NOCNGCON
// #define UCP_NORWND

#if defined(UCP_NOCNGCON) && defined(UCP_NORWND)
#   define UCP_NOUZPSH
#endif

namespace ImplUcp {
    constexpr U32 FloorPow2(U32 uVal) noexcept {
        while (uVal & (uVal - 1))
            uVal -= uVal & (0 - uVal);
        return uVal;
    }

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
#ifndef UCP_NORWND
            x_atmuzAsm.fetch_add(uzSize);
#endif
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
#ifndef UCP_NORWND
            U32 uzRwndAlloc;
#endif
            SegQue qAsm(x_vPool);
            {
                RAII_LOCK(x_mtx);
                if (x_vState(x_kubConnLost))
                    return;
                X_PostRead(std::move(upNewSeg));
#ifndef UCP_NORWND
                uzRwndAlloc =
#endif
                X_InputSegment(qAsm, std::move(upSeg));
            }
#ifndef UCP_NORWND
            x_atmuzAsm.fetch_sub(uzRwndAlloc);
#else
            constexpr U32 uzRwndAlloc = 0;
#endif
            while (!qAsm.IsEmpty()) {
                auto vBuf = MakeBuffer();
#ifndef UCP_NORWND
                auto uzRwndAlloc = qAsm.GetHead()->GetSize();
#endif
                vBuf.PushChunk(qAsm.PopHead());
                while (vBuf.GetTail()->vFlags(kubSegFrg)) {
#ifndef UCP_NORWND
                    uzRwndAlloc += qAsm.GetHead()->GetSize();
#endif
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
#ifndef UCP_NORWND
                constexpr auto ubFlags = x_kubNeedAck | x_kubNeedAsk | x_kubDirty;
#else
                constexpr auto ubFlags = x_kubNeedAck | x_kubDirty;
#endif
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
            PostBuffer(vBuf);
        }
        template<class tPacket>
        inline void PostPacket(tPacket &&vPacket) {
            PostBuffer(MakeBuffer() << vPacket);
        }

    private:
#ifndef UCP_NORWND
        inline U32 X_InputSegment(SegQue &qAsm, UpSeg upRcv) noexcept {
#else
        inline void X_InputSegment(SegQue &qAsm, UpSeg upRcv) noexcept {
#endif
            x_vState += x_kubEchoed;
#ifndef UCP_NORWND
            if (!upRcv->ucRwnd)
                x_vState += x_kubNeedAsk;
            else {
                x_vState -= x_kubNeedAsk | x_kubAsking;
                x_ucAskTimedOut = 0;
                if (!x_ucRmtRwnd)
                    x_vState += x_kubDirty;
            }
            x_ucRmtRwnd = upRcv->ucRwnd;
#endif
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
#ifndef UCP_NOCNGCON
            x_uzFlight -= uzAcked;
#endif
            x_uzSent += uzAcked;
            if (uzAcked) {
                x_vState += x_kubDirty;
                x_unSndAck = x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq;
#ifndef UCP_NOCNGCON
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, kuzMss);
                else
                    x_uzCwnd += std::min(kuzMss * kuzMss / x_uzCwnd, kuzMss);
#endif
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
#ifndef UCP_NORWND
                    U32 uzRwndAlloc = 0;
#endif
                    auto unBound = SeqIncrease(x_unRcvSeq, x_kucBuf);
                    while (x_unRcvSeq != unBound && x_aqRcv[x_unRcvSeq % x_kucBuf]) {
                        x_qAsm.PushTail(std::move(x_aqRcv[x_unRcvSeq % x_kucBuf]));
                        x_unRcvSeq = SeqIncrease(x_unRcvSeq);
                        auto pAsm = x_qAsm.GetTail();
#ifndef UCP_NORWND
                        uzRwndAlloc += pAsm->GetSize();
#endif
                        if (!pAsm->vFlags(kubSegFrg))
                            qAsm.Splice(x_qAsm);
                    }
#ifndef UCP_NORWND
                    return uzRwndAlloc;
#endif
                }
                while (false);
            }
#ifndef UCP_NORWND
            return 0;
#endif
        }

        inline void X_UpdateRtt(UcpSeg *pSeg) noexcept {
            if (pSeg->ucSent != 1)
                return;
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
        }

#ifdef UCP_TRANSMIT
        inline bool X_Flush() {
            X_PrepareSaks();
            X_PrepareQSnd();
            if (x_atmbTsm.test_and_set()) {
                // lag
                x_vState += x_kubDirty;
                return false;
            }
#else
        inline void X_Flush() {
            X_PrepareSaks();
            X_PrepareQSnd();
#endif
            x_vState -= x_kubDirty | x_kubEchoed;
#ifndef UCP_NORWND
            if (x_vState(x_kubAsking)) {
                if (StampDue(x_usNow, x_usAskTimeout)) {
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
#else
            x_usTimeout = StampInfinite(x_usNow);
            constexpr U32 ucRwnd = 0;
#endif
#ifndef UCP_NOCNGCON
            bool bFastResend = false;
            bool bTimedOut = false;
            auto ucSaks = static_cast<U32>(x_vecSndSaks.size());
            auto ucSakSegs = (ucSaks + kuMaxSaks - 1) / kuMaxSaks;
            auto uzSakSegs = ucSakSegs * kuzUcpHdr + ucSaks * 3;
            auto uzCwnd = x_uzCwnd > uzSakSegs ? x_uzCwnd - uzSakSegs : 0;
#endif
#if !defined(UCP_NOCNGCON) && !defined (UCP_NORWND)
            auto uzPshSegs = std::min(uzCwnd, x_ucRmtRwnd * kuzMss);
#elif !defined(UCP_NOCNGCON)
            auto uzPshSegs = uzCwnd;
#elif !defined(UCP_NORWND)
            auto uzPshSegs = x_ucRmtRwnd * kuzMss;
#endif
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
#ifndef UCP_NOUZPSH
                if (pSeg->GetSize() > uzPshSegs) {
                    if (StampBefore(pSeg->usTimeout, x_usTimeout))
                        x_usTimeout = pSeg->usTimeout;
                    continue;
                }
#endif
                if (!pSeg->ucSent) {
                    // first time
#ifndef UCP_NOCNGCON
                    x_uzFlight += pSeg->GetSize();
#endif
#ifndef UCP_NOUZPSH
                    uzPshSegs -= pSeg->GetSize();
#endif
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    if (x_vState(x_kubEchoed))
                        pSeg->ucTimedOut = 0;
                    else if (++pSeg->ucTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
#ifndef UCP_NOCNGCON
                    bTimedOut = true;
#endif
#ifndef UCP_NOUZPSH
                    uzPshSegs -= pSeg->GetSize();
#endif
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
#ifndef UCP_NOCNGCON
                    bFastResend = true;
#endif
#ifndef UCP_NOUZPSH
                    uzPshSegs -= pSeg->GetSize();
#endif
                    X_FlushPayload(pSeg, ucRwnd);
                }
                if (StampBefore(pSeg->usTimeout, x_usTimeout))
                    x_usTimeout = pSeg->usTimeout;
            }
            X_FlushSaks(ucRwnd);
#ifndef UCP_NOCNGCON
            // update cwnd and ssthresh according to RFC5681
            if (bTimedOut) {
                x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd >> 1);
                x_uzCwnd = x_kuzCwndMin;
            }
            else if (bFastResend) {
                x_uzSsthresh = std::max(x_kuzSsthreshMin, std::max(x_uzCwnd, x_uzFlight) >> 1);
                x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuzMss;
            }
#endif
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
        }

        inline void X_FlushPayload(UcpSeg *pSeg, U32 ucRwnd) noexcept {
#ifndef UCP_NORWND
            x_vState -= x_kubNeedAck | x_kubNeedAsk;
#else
            x_vState -= x_kubNeedAck;
#endif
            pSeg->unAck = x_unRcvSeq;
            pSeg->ucRwnd = ucRwnd;
            ++pSeg->ucSent;
            pSeg->ucSkipped = 0;
            pSeg->usSent = x_usNow;
            pSeg->usTimeout = x_usNow + x_utRto;
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
#ifndef UCP_NORWND
                    auto ubFlags = kubSegSak | (x_vState(x_kubNeedAsk) ? kubSegAsk : 0);
                    x_vState -= x_kubNeedAsk;
                    auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, uSaksToSend, ubFlags);
#else
                    auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, uSaksToSend, kubSegSak);
#endif
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
#ifndef UCP_NORWND
            else if (x_vState(x_kubNeedAck | x_kubNeedAsk)) {
                auto ubFlags = x_vState(x_kubNeedAsk) ? kubSegAsk : 0;
                x_vState -= x_kubNeedAck | x_kubNeedAsk;
                auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, 0, ubFlags);
#else
            else if (x_vState(x_kubNeedAck)) {
                x_vState -= x_kubNeedAck;
                auto upSeg = x_vPool.MakeUnique(0, x_unRcvSeq, ucRwnd, 0, 0);
#endif
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
        constexpr static U64 x_kutTick = 8'000;
        constexpr static U32 x_kucFastResend = 2;
        constexpr static U32 x_kucConnLost = 5;
        constexpr static U32 x_kucBuf = FloorPow2((16 << 20) / kuzMss);

#ifndef UCP_NOCNGCON
        constexpr static U32 x_kuzSsthreshInit = 256 << 20;
        constexpr static U32 x_kuzSsthreshMin = 2 * kuzMss;
        constexpr static U32 x_kuzCwndMin = 1 * kuzMss;
        constexpr static U32 x_kuzCwndInit = 3 * kuzMss;

        U32 x_uzSsthresh = x_kuzSsthreshInit;  // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;          // cwnd in RFC5681
        U32 x_uzFlight = 0;                    // FlightSize in RFC 5681
#endif

        constexpr static U32 x_kubConnLost = 0x00000001; // timedout exceeded
        constexpr static U32 x_kubDirty = 0x00000002;    // flush needed
        constexpr static U32 x_kubEchoed = 0x00000004;   // anything received
        constexpr static U32 x_kubNeedAck = 0x00000008;  // payload received, ask received
        constexpr static U32 x_kubStopping = 0x00000010; // shutting down

#ifndef UCP_NORWND
        constexpr static U32 x_kuzAsmMax = 256 << 20;
        constexpr static U32 x_kubAsking = 0x00000020;   // ask pending
        constexpr static U32 x_kubNeedAsk = 0x00000040;  // rmt-rwnd = 0

        U64 x_usAskTimeout = 0;
        U64 x_utAskRto = 0;
        U32 x_ucAskTimedOut = 0;
        U32 x_ucRmtRwnd = x_kuzAsmMax / kuzMss;      // rwnd of the other side, in mss-s
        std::atomic<U32> x_atmuzAsm = x_kuzAsmMax;   // available payload size in x_qAsm and qAsm-s
#endif

        Flags<U32> x_vState = 0; // states
        U64 x_usNow = 0;         // last tick time
        U64 x_usTimeout = 0;     // next time for resend

        U64 x_utRttVar = 0;        // RTTVAR in RFC6298
        U64 x_utSRtt = 0;          // SRTT in RFC6298
        U64 x_utRto = x_kutRtoMax; // RTO in RFC6298

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        UcpBuffer x_qQue {x_vPool}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {x_vPool};    // segments sent waiting for acknowledgement
        UpSeg x_aqRcv[x_kucBuf] {}; // segments received, may not be correctly ordered
        SegQue x_qAsm {x_vPool};    // segments received and ordered, waiting to be reassemble to packets

        U64 x_uzRcvd = 0; // bytes received
        U64 x_uzSent = 0; // bytes sent

        std::atomic<U32> x_atmuzQue = 0;    // bytes of payload in sending queue, for getter use
        std::atomic<U64> x_atmuzRcvd = 0;   // bytes received, for getter use
        std::atomic<U64> x_atmuzSent = 0;   // bytes sent, for getter use

        std::vector<U32> x_vecSndSaks; // saks to be sent

    };

}

using ImplUcp::Ucp;
