#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IntrList.hpp"
#include "SockIo.hpp"
#include "StaticChunk.hpp"
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

        constexpr U32 GetQueueSize() noexcept {
            return x_atmuzPak.load();
        }

        constexpr U32 GetRwndSize() noexcept {
            return x_atmuzRwnd.load();
        }

        constexpr void FreeRwnd(U32 uzPak) noexcept {
            x_atmuzRwnd.fetch_add((uzPak + kuMps) / kuMps * kuShs + uzPak + 1);
        }

     public:
        inline void AssignToIoGroup(IoGroup &vIoGroup) {
            RAII_LOCK(x_mtx);
            x_vLower.AssignToIoGroup(vIoGroup);
            assert(!x_pIoGroup);
            vIoGroup.RegisterTick(*this);
            x_pIoGroup = &vIoGroup;
            for (DWORD i = 0; i < vIoGroup.GetThreads(); ++i)
                X_PostRead(std::make_unique<UcpSeg>(SegRecv {}));
        }

        inline void Shutdown() noexcept {
            RAII_LOCK(x_mtx);
            x_vState += x_kubStopping;
            if (!x_vState(x_kubNeedAck) && x_qPak.IsEmpty() && x_qSnd.IsEmpty())
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
            auto upSeg = std::unique_ptr<UcpSeg>(pSeg);
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
            auto upNewSeg = std::make_unique<UcpSeg>(SegRecv {});
            U32 uzRwndAlloc;
            SegQue qAsm;
            {
                RAII_LOCK(x_mtx);
                if (x_vState(x_kubConnLost))
                    return;
                X_PostRead(std::move(upNewSeg));
                uzRwndAlloc = X_InputSegment(qAsm, std::move(upSeg));
            }
            x_atmuzRwnd.fetch_sub(uzRwndAlloc);
            while (!qAsm.IsEmpty()) {
                auto pSeg = qAsm.GetHead();
                while (pSeg->vFlags(kubSegFrg))
                    pSeg = pSeg->GetNext();
                auto qPak = qAsm.ExtractFromHeadTo(pSeg);
                x_vUpper.OnSegs(qPak);
            }
        }

#ifndef UCP_TRANSMIT
        inline void OnWrite(DWORD dwError, U32 uDone, UcpSeg *pSeg) {
            UNREFERENCED_PARAMETER(dwError);
            UNREFERENCED_PARAMETER(uDone);
            UNREFERENCED_PARAMETER(pSeg);
            // just let the RDT handle this
            if (!pSeg->uzData)
                delete pSeg;
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
            U32 uzQueFree = 0xffffffff;
            {
                RAII_LOCK(x_mtx);
                if (x_vState(x_kubStopping) && x_qPak.IsEmpty() && x_qSnd.IsEmpty()) {
                    x_vLower.Shutdown();
                    return false;
                }
                x_usNow = usNow;
                if (StampDue(x_usNow, x_usSecond)) {
                    ImplDbg::Println(
                        "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                        ", rwnd = ", x_atmuzRwnd.load(), ", xrwnd = ", x_ucRmtRwnd,
                        ", recv = ", x_uzRecv, ", send = ", x_uzSend
                    );
                    x_atmuzRecv = x_uzRecv;
                    x_atmuzSend = x_uzSend;
                    x_uzRecv = 0;
                    x_uzSend = 0;
                    x_usSecond = x_usNow + 1'000'000;
                }
                constexpr auto ubFlags = x_kubNeedAck | x_kubNeedAsk | x_kubDirty;
                if (x_vState(ubFlags) || StampDue(x_usNow, x_usTimeout)) {
                    try {
                        uzQueFree = X_Flush();
                    }
                    catch (ExnIllegalState) {
                        // connection lost
                        x_vState += x_kubConnLost | x_kubStopping;
                        x_vLower.Close();
                        x_vUpper.OnForciblyClose();
                        return false;
                    }
                }
            }
            if (uzQueFree != 0xffffffff) {
                x_atmuzPak.fetch_sub(uzQueFree & 0x7fffffff);
#ifdef UCP_TRANSMIT
                if (!(uzQueFree & 0x80000000)) {
                    if (!x_vecTpes.empty())
                        X_PostTransmit();
                    else
                        x_atmbTsm.clear();
                }
#endif
            }
            return true;
        }

    public:
        template<class tChunk = ByteChunk>
        inline void PostPacket(Byte byPakId, ByteBuffer &vPakBuf, std::unique_ptr<tChunk> upOther = nullptr) {
            auto uzQueAlloc = 1 + vPakBuf.GetSize();
            SegQue qPak {};
            qPak.PushTail(std::make_unique<UcpSeg>());
            auto pSeg = qPak.GetTail();
            pSeg->Write(&byPakId, 1);
            while (!vPakBuf.IsEmpty()) {
                auto upChunk = vPakBuf.PopChunk();
                while (upChunk->IsReadable()) {
                    if (!pSeg->IsWritable()) {
                        pSeg->uzData = pSeg->GetReadable();
                        pSeg->vFlags = kubSegFrg;
                        qPak.PushTail(std::make_unique<UcpSeg>());
                        pSeg = qPak.GetTail();
                    }
                    auto uToWrite = std::min(pSeg->GetWritable(), upChunk->GetReadable());
                    pSeg->Write(upChunk->GetReader(), uToWrite);
                    upChunk->Discard(uToWrite);
                }
            }
            if (upOther) {
                uzQueAlloc += upOther->GetReadable();
                while (upOther->IsReadable()) {
                    if (!pSeg->IsWritable()) {
                        pSeg->uzData = pSeg->GetReadable();
                        pSeg->vFlags = kubSegPsh | kubSegFrg;
                        qPak.PushTail(std::make_unique<UcpSeg>());
                        pSeg = qPak.GetTail();
                    }
                    auto uToWrite = std::min(pSeg->GetWritable(), upOther->GetReadable());
                    pSeg->Write(upOther->GetReader(), uToWrite);
                    upOther->Discard(uToWrite);
                }
            }
            pSeg->uzData = pSeg->GetReadable();
            pSeg->vFlags = kubSegPsh;
            x_atmuzPak.fetch_add(uzQueAlloc);
            RAII_LOCK(x_mtx);
            if (x_vState(x_kubStopping))
                throw ExnIllegalState {};
            x_vState += x_kubDirty;
            x_qPak.Splice(std::move(qPak));
        }

        inline void PostPacket(Byte byPakId, ByteBuffer &&vPakBuf) {
            PostPacket(byPakId, vPakBuf);
        }

    private:
        inline U32 X_InputSegment(SegQue &qAsm, std::unique_ptr<UcpSeg> upRcv) noexcept {
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
                X_UpdateRtt(pSnd);
                uzAcked += pSnd->GetSize();
                x_uzSend += pSnd->GetSize();
                pSnd = pSnd->Remove()->GetNext();
            }
            if (upRcv->vFlags(kubSegSak)) {
                U64 usSakLatest = StampInfinite(x_usNow);
                auto pSak = upRcv->GetReader();
                for (U32 i = 0; i != upRcv->uzData; ++i) {
                    U32 unSak = *reinterpret_cast<U32 *>(pSak) & 0x00ffffff;
                    while (pSnd != x_qSnd.GetNil() && SeqBefore(pSnd->unSeq, unSak))
                        pSnd = pSnd->GetNext();
                    if (pSnd != x_qSnd.GetNil() && pSnd->unSeq == unSak) {
                        if (StampBefore(usSakLatest, pSnd->usSent))
                            usSakLatest = pSnd->usSent;
                        X_UpdateRtt(pSnd);
                        uzAcked += pSnd->GetSize();
                        x_uzSend += pSnd->GetSize();
                        pSnd = pSnd->Remove()->GetNext();
                    }
                    pSak += 3;
                }
                for (pSnd = x_qSnd.GetHead(); pSnd != x_qSnd.GetNil(); pSnd = pSnd->GetNext())
                    if (StampBefore(pSnd->usSent, usSakLatest))
                        ++pSnd->ucSkipped;
            }
            if (uzAcked) {
                x_vState += x_kubDirty;
                x_unSndAck = x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq;
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, kuMss);
                else
                    x_uzCwnd += std::min(kuMss * kuMss / x_uzCwnd, kuMss);
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
                    x_uzRecv += upRcv->GetSize();
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

        inline U32 X_Flush() {
            UCP_DBGOUT("");
            X_PrepareSaks();
            auto uzQueFree = X_PrepareQSnd();
#ifdef UCP_TRANSMIT
            if (x_atmbTsm.test_and_set()) {
                // lag
                x_vState += x_kubDirty;
                return uzQueFree | 0x80000000;
            }
#endif
            x_vState -= x_kubDirty | x_kubEchoed;
            if (x_vState(x_kubAsking)) {
                if (StampDue(x_usNow, x_usAskTimeout)) {
                    ImplDbg::Println("ask timeout");
                    if (++x_ucAskTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
                    x_vState += x_kubNeedAsk;
                    x_utAskRto += x_utAskRto >> 1;
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
            auto uzRwnd = x_atmuzRwnd.load();
            auto ucRwnd = uzRwnd & 0x80000000U ? 0 : uzRwnd / kuMss;
            bool bFastResend = false;
            bool bTimedOut = false;
            auto uzRto = 0;
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (!pSeg->ucSent) {
                    // first time
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    bTimedOut = true;
                    uzRto += pSeg->GetSize();
                    if (x_vState(x_kubEchoed))
                        pSeg->ucTimedOut = 0;
                    else if (++pSeg->ucTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
                    X_FlushPayload(pSeg, ucRwnd);
                }
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
                    uzRto += pSeg->GetSize();
                    bFastResend = true;
                    X_FlushPayload(pSeg, ucRwnd);
                }
                if (StampBefore(pSeg->usTimeout, x_usTimeout))
                    x_usTimeout = pSeg->usTimeout;
            }
            X_FlushSaks(ucRwnd);
            // update cwnd and ssthresh according to RFC5681
            if (bTimedOut) {
                ImplDbg::Println("timeout");
                x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd - uzRto);
                //x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd >> 1);
                x_uzCwnd = x_kuzCwndMin;
            }
            else if (bFastResend) {
                ImplDbg::Println("resend");
                //x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd - uzRto);
                /*x_uzSsthresh = std::max(
                    x_kuzSsthreshMin,
                    (x_qSnd.IsEmpty() ? 0 : static_cast<U32>(x_uzSndIdx - x_qSnd.GetHead()->uzIdx)) >> 1
                );*/
                //x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuMss;
            }
            UCP_DBGOUT(
                "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                ", rwnd = ", x_atmuzRwnd.load(), ", xrwnd = ", x_ucRmtRwnd,
                ", now = ", x_usNow, ", next_timeout = ", x_usTimeout
            );
            return uzQueFree;
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

        inline U32 X_PrepareQSnd() noexcept {
            auto uzSwnd = std::min(x_uzCwnd, x_ucRmtRwnd * kuMss);
            auto uzBound = (x_qSnd.IsEmpty() ? x_uzSndIdx : x_qSnd.GetHead()->uzIdx) + uzSwnd;
            auto unBound = SeqIncrease(x_unSndAck, x_kucBuf);
            U32 uzQueFree = 0;
            while (!x_qPak.IsEmpty() && SeqBefore(x_unSndSeq, unBound) && x_uzSndIdx + x_qPak.GetHead()->GetSize() <= uzBound) {
                auto upSeg = x_qPak.PopHead();
                upSeg->unSeq = std::exchange(x_unSndSeq, SeqIncrease(x_unSndSeq));
                upSeg->uzIdx = x_uzSndIdx;
                upSeg->ucSent = 0;
                upSeg->ucTimedOut = 0;
                x_uzSndIdx += upSeg->GetSize();
                uzQueFree += upSeg->uzData;
                x_qSnd.PushTail(std::move(upSeg));
            }
            UCP_DBGOUT(
                "qsnd.begin = ", uzBound - x_uzCwnd, ", qsnd.end = ", x_uzSndIdx,
                ", bound = ", uzBound
            );
            return uzQueFree;
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
                    auto uSaksToSend = std::min((kuMss - kuShs) / 3, static_cast<U32>(x_vecSndSaks.size()));
                    auto ubFlags = kubSegSak | (x_vState(x_kubNeedAsk) ? kubSegAsk : 0);
                    x_vState -= x_kubNeedAsk;
                    auto upSeg = std::make_unique<UcpSeg>(0, x_unRcvSeq, ucRwnd, uSaksToSend, ubFlags);
                    UCP_DBGOUT("saks = ", uSaksToSend);
                    while (uSaksToSend--) {
                        upSeg->Write(&x_vecSndSaks.back(), 3);
                        x_vecSndSaks.pop_back();
                    }
#ifndef UCP_TRANSMIT
                    X_PostWrite(upSeg.release());
#else
                    X_PushTpe(upSeg.get());
                    x_vecTmpSegs.emplace_back(std::move(upSeg));
#endif
                }
            }
            else if (x_vState(x_kubNeedAck | x_kubNeedAsk)) {
                auto ubFlags = x_vState(x_kubNeedAsk) ? kubSegAsk : 0;
                x_vState -= x_kubNeedAck | x_kubNeedAsk;
                auto upSeg = std::make_unique<UcpSeg>(0, x_unRcvSeq, ucRwnd, 0, ubFlags);
                UCP_DBGOUT("ack = ", x_unRcvSeq);
#ifndef UCP_TRANSMIT
                X_PostWrite(upSeg.release());
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
        inline void X_PostRead(std::unique_ptr<UcpSeg> upSeg) noexcept {
            upSeg->ToBegin();
            auto pSeg = upSeg.release();
            try {
                x_vLower.PostRead(pSeg);
            }
            catch (ExnSockRead<UcpSeg> &) {
                // just let the RDT handle this
                delete pSeg;
            }
            catch (ExnIllegalState) {
                // active shutdown
                delete pSeg;
            }
        }

#ifndef UCP_TRANSMIT
        inline void X_PostWrite(UcpSeg *pSeg) noexcept {
            try {
                x_vLower.Write<UcpSeg>(pSeg);
            }
            catch (ExnSockWrite<UcpSeg> &) {
                // just let the RDT handle this
            }
            catch (ExnIllegalState) {
                // active shutdown
            }
        }
#else
        inline void X_PostTransmit() noexcept {
            try {
                x_vLower.Transmit(x_vecTpes.data(), static_cast<U32>(x_vecTpes.size()), kuMss, &x_vTsmCtx);
            }
            catch (...) {
                OnTransmit(0, 0, nullptr);
            }
        }
#endif

    private:
        Upper &x_vUpper;
        Lower x_vLower;

        Mutex x_mtx;
        IoGroup *x_pIoGroup = nullptr;

#ifdef UCP_TRANSMIT
        ChunkIoContext x_vTsmCtx {};
        std::vector<TRANSMIT_PACKETS_ELEMENT> x_vecTpes;
        std::vector<std::unique_ptr<UcpSeg>> x_vecTmpSegs;
        std::atomic_flag x_atmbTsm {};
#endif

    private:
        constexpr static U64 x_kutRtoMin = 400'000;
        constexpr static U64 x_kutRtoMax = 4'000'000;
        constexpr static U64 x_kutTick = 10'000;
        constexpr static U32 x_kucFastResend = 2;
        constexpr static U32 x_kucConnLost = 5;
        constexpr static U32 x_kuzSsthreshInit = 256 << 20;
        constexpr static U32 x_kuzSsthreshMin = 2 * kuMss;
        constexpr static U32 x_kuzCwndMin = 1 * kuMss;
        constexpr static U32 x_kuzCwndInit = 3 * kuMss;
        constexpr static U32 x_kuzRwndMax = 256 << 20;
        constexpr static U32 x_kucBuf = FloorPow2((16 << 20) / kuMss);

        constexpr static U32 x_kubNeedAck = 0x00000002;  // payload received, ask received
        constexpr static U32 x_kubNeedAsk = 0x00000004;  // rmt-rwnd = 0
        constexpr static U32 x_kubDirty = 0x00000008;    // flush needed
        constexpr static U32 x_kubAsking = 0x00000010;   // ask pending
        constexpr static U32 x_kubEchoed = 0x00000020;   // anything received
        constexpr static U32 x_kubConnLost = 0x40000000; // timedout exceeded
        constexpr static U32 x_kubStopping = 0x80000000; // shutting down

    private:
        Flags<U32> x_vState = 0;
        U64 x_usNow = 0;         // last tick time
        U64 x_usTimeout = 0;     // next time for resend

        U64 x_utRttVar = 0;        // RTTVAR in RFC6298
        U64 x_utSRtt = 0;          // SRTT in RFC6298
        U64 x_utRto = x_kutRtoMax; // RTO in RFC6298

        U32 x_uzSsthresh = x_kuzSsthreshInit; // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;         // cwnd in RFC5681
        U32 x_ucRmtRwnd = x_kuzRwndMax / kuMss;  // rwnd of the other side, in mss-s
        std::atomic<U32> x_atmuzRwnd = x_kuzRwndMax;   // rwnd

        U64 x_usAskTimeout = 0;
        U64 x_utAskRto = 0;
        U32 x_ucAskTimedOut = 0;

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        SegQue x_qPak {}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {}; // segments sent waiting for acknowledgement
        SegQue x_qAsm {}; // segments received and ordered, waiting to be reassemble to packets

        U32 x_uzRecv = 0;                   // bytes received in this second
        U32 x_uzSend = 0;                   // bytes sent in this second
        std::atomic<U32> x_atmuzRecv = 0;   // bytes received in the last second
        std::atomic<U32> x_atmuzSend = 0;   // bytes sent in the last second
        U64 x_usSecond = 0;                 // the end of this second

        std::unique_ptr<UcpSeg> x_aqRcv[x_kucBuf];   // segments received, may not be correctly ordered

        std::atomic<U32> x_atmuzPak = 0;    // total payload size in x_qPak
        U64 x_uzSndIdx = 0; // index of the byte at the end of x_qSnd increased by 1

        std::vector<U32> x_vecSndSaks; // saks to be sent

    };

}

using ImplUcp::Ucp;
