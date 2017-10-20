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

namespace ImplUcp {

#if 0
#   define UCP_DBGOUT(...) DBG_PRINTLN( \
        L"[Ucp ", std::setw(4), std::setfill(L'0'), std::hex, std::uppercase, ((UPtr) this & 0xffff), \
        L"][", std::setw(18), std::setfill(L' '), __func__, "] ", std::dec, __VA_ARGS__ \
    )
#else
#   define UCP_DBGOUT(...) ((void) 0)
#endif

    template<class tUpper, Byte kbyPakIds>
    class Ucp {
    private:
        using Upper = tUpper;
        using Lower = SockIo<Ucp>;

    public:
        Ucp(const Ucp &) = delete;
        Ucp(Ucp &&) = delete;

        template<USize ...kuPakAligns>
        Ucp(
            Upper &vUpper, SOCKET hSocket,
            std::index_sequence<kuPakAligns...>
        ) :
            x_vUpper(vUpper), x_vLower(*this, hSocket),
            x_auPakAligns {kuPakAligns...}
        {
            static_assert(kbyPakIds == sizeof...(kuPakAligns), "aligns");
        }

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
            auto uzRwndFree = (uzPak + kuMps) / kuMps * kuShs + uzPak + 1;
            auto nzRwnd = static_cast<I32>(x_atmuzRwnd.fetch_add(uzRwndFree));
            auto nzRwndFree = static_cast<I32>(uzRwndFree);
            auto nzMss = static_cast<I32>(kuMss);
            if (nzRwnd < nzMss && nzRwnd + nzRwndFree >= nzMss) {
                RAII_LOCK(x_mtx);
                x_bNeedAck = true;
            }
        }

     public:
        inline void AssignToIoGroup(IoGroup &vIoGroup) {
            RAII_LOCK(x_mtx);
            x_vLower.AssignToIoGroup(vIoGroup);
            assert(!x_pIoGroup);
            vIoGroup.RegisterTick(*this);
            x_pIoGroup = &vIoGroup;
            for (DWORD i = 0; i < vIoGroup.GetThreads(); ++i)
                X_PostRead(std::make_unique<SegPayload>(SegRecv {}));
        }

        inline void Shutdown() noexcept {
            RAII_LOCK(x_mtx);
            x_bStopping = true;
            if (!x_bNeedAck && x_qPak.IsEmpty() && x_qSnd.IsEmpty())
                x_vLower.Shutdown();
        }

        inline void Close() noexcept {
            RAII_LOCK(x_mtx);
            x_bStopping = true;
            if (x_pIoGroup)
                x_pIoGroup->UnregisterTick(*this);
            x_vLower.Close();
        }

        void OnFinalize() noexcept {
            RAII_LOCK(x_mtx);
            x_bStopping = true;
            x_vUpper.OnFinalize();
        }

        inline void OnRead(DWORD dwError, U32 uDone, SegPayload *pSeg) {
            auto upSeg = std::unique_ptr<SegPayload>(pSeg);
            if (dwError) {
                // just let the RDT handle this
                {
                    RAII_LOCK(x_mtx);
                    if (x_bBroken)
                        return;
                }
                X_PostRead(std::move(upSeg));
                return;
            }
            assert(uDone);
            assert(uDone == pSeg->GetReadable());
            pSeg->Decode();
            U32 uzRwndAlloc;
            SegQue qAsm;
            {
                RAII_LOCK(x_mtx);
                if (x_bBroken)
                    return;
                X_PostRead(std::make_unique<SegPayload>(SegRecv {}));
                uzRwndAlloc = X_OnRead(qAsm, std::move(upSeg));
            }
            x_atmuzRwnd.fetch_sub(uzRwndAlloc);
            X_AssemblePackets(qAsm);
        }

        inline void OnWrite(DWORD dwError, U32 uDone, SegPayload *pSeg) {
            UNREFERENCED_PARAMETER(dwError);
            UNREFERENCED_PARAMETER(uDone);
            UNREFERENCED_PARAMETER(pSeg);
            // just let the RDT handle this
            if (!pSeg->uzData)
                delete pSeg;
        }

        bool OnTick(U64 usNow) noexcept {
            RAII_LOCK(x_mtx);
            if (x_bStopping && x_qPak.IsEmpty() && x_qSnd.IsEmpty()) {
                x_vLower.Shutdown();
                return false;
            }
            x_usNow = usNow;
            if (StampDue(x_usNow, x_usDue1s)) {
                ImplDbg::Println(
                    "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                    ", rwnd = ", x_atmuzRwnd.load(), ", xrwnd = ", x_ucRwnd,
                    ", recv = ", x_uzRecv, ", send = ", x_uzSend
                );
                x_atmuzRecv = x_uzRecv;
                x_atmuzSend = x_uzSend;
                x_uzRecv = 0;
                x_uzSend = 0;
                x_usDue1s = x_usNow + 1'000'000;
            }
            if (x_bDirty || x_bNeedAck || x_bEchoed || StampDue(x_usNow, x_usTimeout)) {
                try {
                    X_Flush();
                }
                catch (ExnIllegalState) {
                    // connection lost
                    x_bBroken = true;
                    x_bStopping = true;
                    x_vLower.Close();
                    x_vUpper.OnForciblyClose();
                    return false;
                }
                x_bDirty = false;
                x_bEchoed = false;
            }
            return true;
        }

    public:
        template<class tChunk = ByteChunk>
        inline void PostPacket(Byte byPakId, ByteBuffer &vPakBuf, std::unique_ptr<tChunk> upOther = nullptr) {
            if (byPakId >= kbyPakIds)
                throw ExnIllegalArg {};
            auto uzQueAlloc = 1 + vPakBuf.GetSize();
            SegQue qPak {};
            qPak.PushTail(std::make_unique<SegPayload>());
            auto pSeg = qPak.GetTail();
            pSeg->Write(&byPakId, 1);
            while (!vPakBuf.IsEmpty()) {
                auto upChunk = vPakBuf.PopChunk();
                while (upChunk->IsReadable()) {
                    if (!pSeg->IsWritable()) {
                        pSeg->uzData = pSeg->GetReadable();
                        pSeg->ucFrag = 1;
                        qPak.PushTail(std::make_unique<SegPayload>());
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
                        pSeg->ucFrag = 1;
                        qPak.PushTail(std::make_unique<SegPayload>());
                        pSeg = qPak.GetTail();
                    }
                    auto uToWrite = std::min(pSeg->GetWritable(), upOther->GetReadable());
                    pSeg->Write(upOther->GetReader(), uToWrite);
                    upOther->Discard(uToWrite);
                }
            }
            pSeg->uzData = pSeg->GetReadable();
            pSeg->ucFrag = 0;
            RAII_LOCK(x_mtx);
            if (x_bStopping)
                throw ExnIllegalState {};
            x_bDirty = true;
            x_qPak.Splice(std::move(qPak));
            x_atmuzPak.fetch_add(uzQueAlloc);
        }

        inline void PostPacket(Byte byPakId, ByteBuffer &&vPakBuf) {
            PostPacket(byPakId, vPakBuf);
        }

        // OnPacket(Byte byPakId, ByteBuffer vPakBuf);

    private:
        /*
        template<class tChunk>
        static inline U32 X_DecodeFrame(
            U32 &unAck, U32 &ucRwnd, SegQue &qDsm,
            std::vector<U32> &vecSaks, tChunk *pChunk
        ) noexcept {
            while (pChunk->IsReadable()) {
                auto upSeg = std::make_unique<SegPayload>(pChunk);
                unAck = upSeg->unAck;
                ucRwnd = upSeg->ucRwnd;
                for (auto i = upSeg->ucSaks; i; --i) {
                    assert(pChunk->GetReadable() >= 3);
                    U32 uSak = 0;
                    pChunk->Read(&uSak, 3);
                    vecSaks.emplace_back(uSak);
                }
                if (upSeg->uzData) {
                    pChunk->Read(upSeg->GetWriter(), upSeg->uzData);
                    upSeg->Skip(upSeg->uzData);
                    qDsm.PushTail(std::move(upSeg));
                }
            }
            assert(!pChunk->IsReadable());
            return unAck;
        }*/

        inline U32 X_OnRead(SegQue &qAsm, std::unique_ptr<SegPayload> upRcv) noexcept {
            x_ucRwnd = upRcv->ucRwnd;
            U32 uzAcked = 0;
            auto pSnd = x_qSnd.GetHead();
            while (pSnd != x_qSnd.GetNil() && SeqBefore(pSnd->unSeq, upRcv->unAck)) {
                X_UpdateRtt(pSnd);
                uzAcked += pSnd->GetSize();
                x_uzSend += pSnd->GetSize();
                pSnd = pSnd->Remove()->GetNext();
            }
            if (upRcv->ucSaks) {
                U64 usSakLatest = StampInfinite(x_usNow);
                auto pSak = upRcv->GetReader();
                for (U32 i = 0; i != upRcv->ucSaks; ++i) {
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
            x_bEchoed = uzAcked;
            auto unOldAck = std::exchange(x_unSndAck, x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq);
            if (unOldAck != x_unSndAck) {
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, kuMss);
                else
                    x_uzCwnd += std::min(kuMss * kuMss / x_uzCwnd, kuMss);
            }
            if(upRcv->uzData) {
                do {
                    if (!SeqBefore(upRcv->unSeq, x_unRcvSeq + x_kucBuf))
                        break;
                    x_bNeedAck = true;
                    x_vecSndSaks.emplace_back(upRcv->unSeq);
                    if (SeqBefore(upRcv->unSeq, x_unRcvSeq))
                        break;
                    auto &upQue = x_aRcv[upRcv->unSeq % x_kucBuf];
                    if (upQue)
                        continue;
                    x_uzRecv += upRcv->GetSize();
                    upQue = std::move(upRcv);
                    U32 uzRwndAlloc = 0;
                    auto unBound = SeqIncrease(x_unRcvSeq, x_kucBuf);
                    while (x_unRcvSeq != unBound && x_aRcv[x_unRcvSeq % x_kucBuf]) {
                        x_qAsm.PushTail(std::move(x_aRcv[x_unRcvSeq % x_kucBuf]));
                        x_unRcvSeq = SeqIncrease(x_unRcvSeq);
                        auto pAsm = x_qAsm.GetTail();
                        uzRwndAlloc += pAsm->GetSize();
                        if (!pAsm->ucFrag)
                            qAsm.Splice(x_qAsm);
                    }
                    return uzRwndAlloc;
                }
                while (false);
            }
            return 0;
        }

        /*
        inline U32 X_OnRead(
            SegQue &qAsm, U32 unAck, U32 ucRwnd,
            SegQue &qDsm, std::vector<U32> &vecSaks
        ) noexcept {
            x_ucRwnd = ucRwnd;
            U64 usSakLatest = StampInfinite(x_usNow);
            U32 uzAcked = 0;
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && SeqBefore(pSeg->unSeq, unAck)) {
                X_UpdateRtt(pSeg);
                uzAcked += pSeg->GetSize();
                x_uzSend += pSeg->GetSize();
                pSeg = pSeg->Remove()->GetNext();
            }
            for (auto unSak : vecSaks) {
                while (pSeg != x_qSnd.GetNil() && SeqBefore(pSeg->unSeq, unSak))
                    pSeg = pSeg->GetNext();
                if (pSeg != x_qSnd.GetNil() && pSeg->unSeq == unSak) {
                    if (StampBefore(usSakLatest, pSeg->usSent))
                        usSakLatest = pSeg->usSent;
                    X_UpdateRtt(pSeg);
                    uzAcked += pSeg->GetSize();
                    x_uzSend += pSeg->GetSize();
                    pSeg = pSeg->Remove()->GetNext();
                }
            }
            for (pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (StampBefore(pSeg->usSent, usSakLatest))
                    ++pSeg->ucSkipped;
            }
            auto unOldAck = std::exchange(x_unSndAck, x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq);
            if (unOldAck != x_unSndAck) {
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, kuMss);
                else
                    x_uzCwnd += std::min(kuMss * kuMss / x_uzCwnd, kuMss);
            }
            x_bEchoed = uzAcked;
            while (!qDsm.IsEmpty()) {
                x_bDirty = true;
                auto upDsm = qDsm.PopHead();
                if (!SeqBefore(upDsm->unSeq, x_unRcvSeq + x_kucBuf))
                    continue;
                x_bNeedAck = true;
                x_vecSndSaks.emplace_back(upDsm->unSeq);
                if (SeqBefore(upDsm->unSeq, x_unRcvSeq))
                    continue;
                auto &upRcv = x_aRcv[upDsm->unSeq % x_kucBuf];
                if (upRcv)
                    continue;
                x_uzRecv += upDsm->GetSize();
                upRcv = std::move(upDsm);
            }
            U32 uzRwndAlloc = 0;
            auto unBound = SeqIncrease(x_unRcvSeq, x_kucBuf);
            while (x_unRcvSeq != unBound && x_aRcv[x_unRcvSeq % x_kucBuf]) {
                x_qAsm.PushTail(std::move(x_aRcv[x_unRcvSeq % x_kucBuf]));
                x_unRcvSeq = SeqIncrease(x_unRcvSeq);
                auto pAsm = x_qAsm.GetTail();
                uzRwndAlloc += pAsm->GetSize();
                if (!pAsm->ucFrag)
                    qAsm.Splice(x_qAsm);
            }
            return uzRwndAlloc;
        }*/

        inline void X_UpdateRtt(SegPayload *pSeg) noexcept {
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

        // no lock acquired
        inline void X_AssemblePackets(SegQue &qAsm) noexcept {
            while (!qAsm.IsEmpty()) {
                auto pSeg = qAsm.GetHead();
                while (pSeg->ucFrag)
                    pSeg = pSeg->GetNext();
                auto qPak = qAsm.ExtractFromHeadTo(pSeg);
                x_vUpper.OnSegs(qPak);
            }
            /*
            while (!qAsm.IsEmpty()) {
                auto pSeg = qAsm.GetHead();
                Byte byPakId;
                pSeg->Read(&byPakId, 1);
                U32 uzPak = pSeg->GetReadable();
                while (pSeg->ucFrag) {
                    pSeg = pSeg->GetNext();
                    uzPak += pSeg->GetReadable();
                }
                if (uzPak) {
                    auto upChunk = ByteChunk::MakeUnique(uzPak, x_auPakAligns[byPakId]);
                    auto upSeg = qAsm.PopHead();
                    upChunk->Write(upSeg->GetReader(), upSeg->GetReadable());
                    while (upSeg->ucFrag) {
                        upSeg = qAsm.PopHead();
                        upChunk->Write(upSeg->GetReader(), upSeg->GetReadable());
                    }
                    UCP_DBGOUT("pak.id = ", (int) byPakId, ", pak.size = ", uzPak);
                    x_vUpper.OnPacket(byPakId, ByteBuffer(std::move(upChunk)));
                }
                else {
                    auto upSeg = qAsm.PopHead();
                    UCP_DBGOUT("pak.id = ", (int) byPakId, ", pak.size = 0");
                    x_vUpper.OnPacket(byPakId, ByteBuffer());
                }
            }*/
        }

        inline void X_Flush() {
            UCP_DBGOUT("");
            X_PrepareSaks();
            X_PrepareQSnd();
            auto uzRwnd = x_atmuzRwnd.load();
            auto ucRwnd = uzRwnd & 0x80000000U ? 0 : uzRwnd / kuMss;
            bool bFastResend = false;
            bool bTimedOut = false;
            x_usTimeout = StampInfinite(x_usNow);
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (!pSeg->ucSent) {
                    // first time
                    X_EncodeSegment(pSeg, ucRwnd);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    bTimedOut = true;
                    if (x_bEchoed)
                        pSeg->ucTimedOut = 0;
                    else if (++pSeg->ucTimedOut >= x_kucConnLost)
                        throw ExnIllegalState {};
                    X_EncodeSegment(pSeg, ucRwnd);
                }
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
                    bFastResend = true;
                    X_EncodeSegment(pSeg, ucRwnd);
                }
                if (StampBefore(pSeg->usTimeout, x_usTimeout))
                    x_usTimeout = pSeg->usTimeout;
            }
            X_EncodeSaks(ucRwnd);
            // update cwnd and ssthresh according to RFC5681
            if (bTimedOut) {
                ImplDbg::Println("timeout");
                x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd >> 1);
                x_uzCwnd = x_kuzCwndMin;
            }
            else if (bFastResend) {
                ImplDbg::Println("resend");
                x_uzSsthresh = std::max(
                    x_kuzSsthreshMin,
                    (x_qSnd.IsEmpty() ? 0 : static_cast<U32>(x_uzSndIdx - x_qSnd.GetHead()->uzIdx)) >> 1
                );
                x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuMss;
            }
            UCP_DBGOUT(
                "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                ", rwnd = ", x_atmuzRwnd.load(), ", xrwnd = ", x_ucRwnd,
                ", now = ", x_usNow, ", next_timeout = ", x_usTimeout
            );
        }

        inline void X_PrepareSaks() noexcept {
            std::sort(x_vecSndSaks.begin(), x_vecSndSaks.end(), std::greater<U32> {});
            U32 ucSaks = 0;
            for (auto unSak : x_vecSndSaks)
                if (SeqBefore(x_unRcvSeq, unSak) && (!ucSaks || unSak != x_vecSndSaks[ucSaks - 1]))
                    x_vecSndSaks[ucSaks++] = unSak;
            x_vecSndSaks.resize(ucSaks);
            UCP_DBGOUT("saks = ", x_vecSndSaks);
        }

        inline void X_PrepareQSnd() noexcept {
            auto uzSwnd = std::min(x_uzCwnd, x_ucRwnd * kuMss);
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
            x_atmuzPak.fetch_sub(uzQueFree);
            UCP_DBGOUT(
                "qsnd.begin = ", uzBound - x_uzCwnd, ", qsnd.end = ", x_uzSndIdx,
                ", bound = ", uzBound
            );
        }

        inline void X_EncodeSegment(SegPayload *pSeg, U32 ucRwnd) noexcept {
            x_bNeedAck = false;
            pSeg->unAck = x_unRcvSeq;
            pSeg->ucRwnd = ucRwnd;
            pSeg->ucSaks = 0;
            ++pSeg->ucSent;
            pSeg->ucSkipped = 0;
            pSeg->usSent = x_usNow;
            pSeg->usTimeout = x_usNow + x_utRto;
            UCP_DBGOUT("seq = ", pSeg->unSeq, ", ack = ", pSeg->unAck,
                ", timedout = ", pSeg->ucTimedOut, ", timeout = ", pSeg->usTimeout
            );
            pSeg->EncodeHdr();
            X_PostWrite(pSeg);
        }

        inline void X_EncodeSaks(U32 ucRwnd) noexcept {
            if (!x_vecSndSaks.empty()) {
                x_bNeedAck = false;
                while (!x_vecSndSaks.empty()) {
                    auto uSaksToSend = std::min((kuMss - kuShs) / 3, static_cast<U32>(x_vecSndSaks.size()));
                    auto upSeg = std::make_unique<SegPayload>(0, x_unRcvSeq, ucRwnd, uSaksToSend, 0, 0);
                    UCP_DBGOUT("saks = ", uSaksToSend);
                    while (uSaksToSend--) {
                        upSeg->Write(&x_vecSndSaks.back(), 3);
                        x_vecSndSaks.pop_back();
                    }
                    X_PostWrite(upSeg.release());
                }
            }
            else if (x_bNeedAck) {
                x_bNeedAck = false;
                auto upSeg = std::make_unique<SegPayload>(0, x_unRcvSeq, ucRwnd, 0, 0, 0);
                UCP_DBGOUT("ack = ", x_unRcvSeq);
                X_PostWrite(upSeg.release());
            }
        }

    private:
        inline void X_PostRead(std::unique_ptr<SegPayload> upSeg) noexcept {
            upSeg->ToBegin();
            auto pSeg = upSeg.release();
            try {
                x_vLower.PostRead(pSeg);
            }
            catch (ExnSockRead<SegPayload> &) {
                // just let the RDT handle this
                delete pSeg;
            }
            catch (ExnIllegalState) {
                // active shutdown
                delete pSeg;
            }
        }

        inline void X_PostWrite(SegPayload *pSeg) noexcept {
            try {
                x_vLower.Write<SegPayload>(pSeg);
            }
            catch (ExnSockWrite<SegPayload> &) {
                // just let the RDT handle this
            }
            catch (ExnIllegalState) {
                // active shutdown
            }
        }

    private:
        Upper &x_vUpper;
        Lower x_vLower;
        
        const USize x_auPakAligns[kbyPakIds];

    private:
        RecursiveMutex x_mtx;
        IoGroup *x_pIoGroup = nullptr;
        bool x_bStopping = false;

        //std::unique_ptr<ByteChunk> x_upFrameRcv = ByteChunk::MakeUnique(kuMss);
        //std::unique_ptr<StaticChunk<kuMss>> x_upFrameSnd = std::make_unique<StaticChunk<kuMss>>();

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
        constexpr static U32 x_kuzRwndMax = 64 << 20;
        constexpr static U32 x_kucBuf = 256;

    private:
        bool x_bBroken = false;  // whether Ucp is closed due to timed out
        bool x_bDirty = false;   // whether flush is needed
        bool x_bNeedAck = false; // whether an empty ack segment is needed
        bool x_bEchoed = false;  // whether any data has been received
        U64 x_usNow = 0;         // last tick time
        U64 x_usTimeout = 0;     // next time for resend

        U64 x_utRttVar = 0;        // RTTVAR in RFC6298
        U64 x_utSRtt = 0;          // SRTT in RFC6298
        U64 x_utRto = x_kutRtoMax; // RTO in RFC6298

        U32 x_uzSsthresh = x_kuzSsthreshInit; // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;  // cwnd in RFC5681
        U32 x_ucRwnd = x_kuzRwndMax / kuMss;
        std::atomic<U32> x_atmuzRwnd = x_kuzRwndMax;

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        SegQue x_qPak {}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {}; // segments sent waiting for acknowledgement
        SegQue x_qAsm {}; // segments received and ordered, waiting to be reassemble to packets

        U32 x_uzRecv = 0;
        U32 x_uzSend = 0;
        std::atomic<U32> x_atmuzRecv = 0;
        std::atomic<U32> x_atmuzSend = 0;
        U64 x_usDue1s = 0;

        std::unique_ptr<SegPayload> x_aRcv[x_kucBuf];

        std::atomic<U32> x_atmuzPak = 0;    // total payload size in x_qPak
        U64 x_uzSndIdx = 0; // index of the byte at the end of x_qSnd increased by 1

        std::vector<U32> x_vecSndSaks; // saks to be sent

    };

}

using ImplUcp::Ucp;
