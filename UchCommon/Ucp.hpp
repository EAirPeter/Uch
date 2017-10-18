#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IntrList.hpp"
#include "SockIo.hpp"
#include "StaticChunk.hpp"
#include "Sync.hpp"

#include "Debug.hpp"

namespace ImplUcp {
    //  0                   1                   2                   3
    //  0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | unSeq                       | unAck                       | ucRwnd            |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | ucSaks       | uzData       | ucFrag  | abyPadding (will not be sent)         |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | unSaks (selective acknowledgements, 3 bytes per sak, ucSaks sak-s in total)   |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | abyData (payload, uzData bytes in total)                                      |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+

    // unSeq is valid if ucSize is not 0
    // if ucSize is 0, the segment does not need acknowledgement
    // unAck means any segment with unSeq < unAck has been acknowledged for sender
    // any one of unSaks should be greater than unAck

    // uc: number of count
    // un: sequence number, for: seq, ack, sak
    // us: time stamp in microseconds according to QPC
    // ut: time duration in microseconds
    // uw: size of window in count of segments
    // uz: size in bytes
    //  q: segment queue

    constexpr static USize kuMss = 1400;
    constexpr static USize kuShs = 12;
    constexpr static USize kuMps = kuMss - kuShs;
    constexpr static USize kuMks = kuMps * 256;

    struct SegHdr {
        constexpr SegHdr() noexcept = default;
        constexpr SegHdr(const SegHdr &) noexcept = default;

        constexpr SegHdr(U32 unSeq_, U32 unAck_, U32 ucRwnd_, U32 ucSaks_, U32 uzData_, U32 ucFrag_) noexcept :
            unSeq(unSeq_), unAck(unAck_), ucRwnd(ucRwnd_), ucSaks(ucSaks_), uzData(uzData_), ucFrag(ucFrag_) {}

        inline SegHdr(ByteChunk *pChunk) noexcept {
            // decode
            assert(pChunk->GetReadable() >= kuShs);
            pChunk->Read(&unSeq, 3);
            pChunk->Read(&unAck, 3);
            pChunk->Read(&ucRwnd, 2);
            U32 uTmp = 0;
            pChunk->Read(&uTmp, 3);
            ucSaks = uTmp & 0x00000fff;
            uzData = uTmp >> 12;
            pChunk->Read(&ucFrag, 1);
        }

        U32 unSeq = 0;  // 24-bit sequence number
        U32 unAck = 0;  // 24-bit acknowledgement number
        U32 ucRwnd = 0; // 16-bit size of sender's rwnd in count of mss-s
        U32 ucSaks = 0; // 12-bit count of saks
        U32 uzData = 0; // 12-bit size of payload in bytes
        U32 ucFrag = 0; //  8-bit fragment number

        inline void Encode(ByteChunk *pChunk) noexcept {
            assert(pChunk->GetWritable() >= kuShs);
            assert(!(unSeq & 0xff000000));
            assert(!(unAck & 0xff000000));
            assert(!(ucRwnd & 0xffff0000));
            assert(!(ucSaks & 0xfffff000));
            assert(!(uzData & 0xfffff000));
            assert(!(ucFrag & 0xffffff00));
            pChunk->Write(&unSeq, 3);
            pChunk->Write(&unAck, 3);
            pChunk->Write(&ucRwnd, 2);
            U32 uTmp = uzData << 12 | ucSaks;
            pChunk->Write(&uTmp, 3);
            pChunk->Write(&ucFrag, 1);
        }

    };

    constexpr bool SeqBefore(U32 unSub, U32 unObj) noexcept {
        return (unSub - unObj) & 0x01000000;
    }

    constexpr U32 SeqIncrease(U32 unSeq) noexcept {
        return (unSeq + 1) & 0x00ffffff;
    }

    struct SegPayload : SegHdr, StaticChunk<kuMps>, IntrListNode<SegPayload> {
        inline SegPayload() noexcept = default;
        inline SegPayload(const SegHdr &vHdr) noexcept : SegHdr(vHdr), StaticChunk(), IntrListNode() {}

        constexpr USize GetSize() noexcept {
            return kuShs + StaticChunk::GetReadable();
        }

        U64 uzIdx;          // index of the first byte
        U32 ucSent;         // sent count
        U32 ucTimedOut;     // timed out count
        U32 ucSkipped;      // skipped by sak count
        U64 usSent;         // when the segment is sent
        U64 usTimeout;      // when the segment should be resent due to timed out

    };

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
        using SegQue = IntrList<SegPayload>;

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
        inline void AssignToIoGroup(IoGroup &vIoGroup) {
            RAII_LOCK(x_mtx);
            x_vLower.AssignToIoGroup(vIoGroup);
            assert(!x_pIoGroup);
            vIoGroup.RegisterTick(*this);
            x_pIoGroup = &vIoGroup;
            X_PostRead();
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

        inline void OnRead(DWORD dwError, USize uDone, ByteChunk *pChunk) {
            SegQue qAsm;
            {
                RAII_LOCK(x_mtx);
                if (x_bBroken)
                    return;
                if (dwError) {
                    // just let the RDT handle this
                    // X_OnError(static_cast<int>(dwError));
                    X_PostRead();
                    return;
                }
                assert(uDone == pChunk->GetReadable());
                assert(pChunk == x_upFrameRcv.get());
                X_DecodeFrame(qAsm);
                X_PostRead();
            }
            X_AssemblePackets(qAsm);
        }

        inline void OnWrite(DWORD dwError, USize uDone, std::unique_ptr<ByteChunk> upChunk) {
            UNREFERENCED_PARAMETER(dwError);
            UNREFERENCED_PARAMETER(uDone);
            UNREFERENCED_PARAMETER(upChunk);
            // just let the RDT handle this
        }

        bool OnTick(U64 usNow) noexcept {
            RAII_LOCK(x_mtx);
            if (x_bStopping && x_qPak.IsEmpty() && x_qSnd.IsEmpty()) {
                x_vLower.Shutdown();
                return false;
            }
            x_usNow = usNow;
            if (x_bDirty || x_bEchoed || StampDue(x_usNow, x_usTimeout)) {
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
        inline void PostPacket(Byte byPakId, ByteBuffer &vPakBuf) {
            if (byPakId >= kbyPakIds)
                throw ExnIllegalArg {};
            auto uSize = 1 + vPakBuf.GetSize();
            if (uSize > kuMks)
                throw ExnIllegalArg {};
            auto uSegs = static_cast<U32>((uSize + kuMps - 1) / kuMps);
            assert(uSegs <= 256);
            SegQue qPak {};
            qPak.PushTail(std::make_unique<SegPayload>());
            auto pSeg = qPak.GetTail();
            pSeg->ucFrag = --uSegs;
            pSeg->Write(&byPakId, 1);
            while (!vPakBuf.IsEmpty()) {
                auto upChunk = vPakBuf.PopChunk();
                while (upChunk->IsReadable()) {
                    if (!pSeg->IsWritable()) {
                        pSeg->uzData = static_cast<U32>(pSeg->GetReadable());
                        qPak.PushTail(std::make_unique<SegPayload>());
                        pSeg = qPak.GetTail();
                        pSeg->ucFrag = --uSegs;
                    }
                    auto uToWrite = std::min(pSeg->GetWritable(), upChunk->GetReadable());
                    pSeg->Write(upChunk->GetReader(), uToWrite);
                    upChunk->Discard(uToWrite);
                }
            }
            pSeg->uzData = static_cast<U32>(pSeg->GetReadable());
            assert(!uSegs);
            RAII_LOCK(x_mtx);
            if (x_bStopping)
                throw ExnIllegalState {};
            x_bDirty = true;
            x_qPak.Splice(std::move(qPak));
        }

        inline void PostPacket(Byte byPakId, ByteBuffer &&vPakBuf) {
            PostPacket(byPakId, vPakBuf);
        }

        // OnPacket(Byte byPakId, ByteBuffer vPakBuf);

    private:
        void X_DecodeFrame(SegQue &qAsm) noexcept {
            UCP_DBGOUT("size = ", x_upFrameRcv->GetReadable());
            U32 unOldSndAck = x_unSndAck;
            U64 usSakLatest = StampInfinite(x_usNow);
            U32 uzAcked = 0;
            while (x_upFrameRcv->IsReadable()) {
                x_bEchoed = true;
                SegHdr vHdr(x_upFrameRcv.get());
                X_OnAck(uzAcked, vHdr.unAck);
                for (auto i = vHdr.ucSaks; i; --i) {
                    assert(x_upFrameRcv->GetReadable() >= 3);
                    U32 uSak = 0;
                    x_upFrameRcv->Read(&uSak, 3);
                    X_OnSak(uzAcked, usSakLatest, uSak);
                }
                if (vHdr.uzData)
                    X_OnPsh(vHdr);
            }
            assert(!x_upFrameRcv->IsReadable());
            X_UpdateQSnd(usSakLatest);
            X_UpdateQRcv(qAsm);
            // update cwnd and ssthresh according to RFC5681
            if (SeqBefore(unOldSndAck, x_unSndAck)) {
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, static_cast<U32>(kuMss));
                else
                    x_uzCwnd += std::min(static_cast<U32>(kuMss * kuMss / x_uzCwnd), static_cast<U32>(kuMss));
            }
            UCP_DBGOUT("cwnd = ", x_uzCwnd, ", now = ", x_usNow);
        }

        inline void X_OnAck(U32 &uzAcked, U32 unAck) noexcept {
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && SeqBefore(pSeg->unSeq, unAck)) {
                UCP_DBGOUT(
                    "ack = ", unAck, ", seq = ", pSeg->unSeq,
                    ", rtt = ", x_usNow - pSeg->usSent, ", sent = ", pSeg->ucSent
                );
                X_UpdateRtt(pSeg);
                uzAcked += static_cast<U32>(pSeg->GetSize());
                pSeg = pSeg->Remove()->GetNext();
            }
        }

        inline void X_OnSak(U32 &uzAcked, U64 &usSakLatest, U32 unSak) noexcept {
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && SeqBefore(pSeg->unSeq, unSak))
                pSeg = pSeg->GetNext();
            if (pSeg != x_qSnd.GetNil() && pSeg->unSeq == unSak) {
                UCP_DBGOUT(
                   "sak = ", unSak, ", seq = ", pSeg->unSeq,
                    ", rtt = ", x_usNow - pSeg->usSent, ", sent = ", pSeg->ucSent
                );
                X_UpdateRtt(pSeg);
                if (StampBefore(usSakLatest, pSeg->usSent))
                    usSakLatest = pSeg->usSent;
                uzAcked += static_cast<U32>(pSeg->GetSize());
                pSeg->Remove();
            }
            else {
                UCP_DBGOUT("sak = ", unSak, ", skipped");
            }
        }

        inline void X_OnPsh(const SegHdr &vHdr) noexcept {
            x_bNeedAck = true;
            x_vecSndSaks.emplace_back(vHdr.unSeq);
            auto uSize = static_cast<USize>(vHdr.uzData);
            UCP_DBGOUT("seq = ", vHdr.unSeq, ", size = ", uSize);
            assert(x_upFrameRcv->GetReadable() >= vHdr.uzData);
            if (SeqBefore(vHdr.unSeq, x_unRcvSeq)) {
                // ack may lost, resend ack on next tick
                x_upFrameRcv->Discard(uSize);
                return;
            }
            auto pSeg = x_qRcv.GetTail();
            while (pSeg != x_qRcv.GetNil() && SeqBefore(vHdr.unSeq, pSeg->unSeq))
                pSeg = pSeg->GetPrev();
            if (pSeg != x_qRcv.GetNil() && pSeg->unSeq == vHdr.unSeq) {
                // duplicated segment, ack may lost, resend ack on next tick
                x_upFrameRcv->Discard(uSize);
                return;
            }
            auto upSeg = std::make_unique<SegPayload>(std::move(vHdr));
            assert(uSize <= upSeg->GetWritable());
            x_upFrameRcv->Read(upSeg->GetWriter(), uSize);
            upSeg->Skip(uSize);
            // payload retrieved, insert to x_qRcv
            pSeg->InsertAfter(std::move(upSeg));
        }

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

        inline void X_UpdateQSnd(U64 usSakLatest) noexcept {
            x_unSndAck = x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq;
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (StampDue(usSakLatest, pSeg->usSent))
                    ++pSeg->ucSkipped;
            }
            UCP_DBGOUT("snd.ack = ", x_unSndAck);
        }

        inline void X_UpdateQRcv(SegQue &qAsm) noexcept {
            auto pSeg = x_qRcv.GetHead();
            while (pSeg != x_qRcv.GetNil() && pSeg->unSeq == x_unRcvSeq) {
                x_qAsm.PushTail(pSeg->Remove());
                auto pAsm = x_qAsm.GetTail();
                assert(pAsm->GetReadable() == pAsm->uzData);
                if (!pAsm->ucFrag)
                    qAsm.Splice(x_qAsm);
                x_unRcvSeq = SeqIncrease(x_unRcvSeq);
                pSeg = x_qRcv.GetHead();
            }
            UCP_DBGOUT("rcv.seq = ", x_unRcvSeq);
        }

        // no lock acquired
        inline void X_AssemblePackets(SegQue &qAsm) noexcept {
            while (!qAsm.IsEmpty()) {
                auto pSeg = qAsm.GetHead();
                Byte byPakId;
                pSeg->Read(&byPakId, 1);
                USize uPakSize = pSeg->GetReadable();
                while (pSeg->ucFrag) {
                    pSeg = pSeg->GetNext();
                    uPakSize += pSeg->GetReadable();
                }
                if (uPakSize) {
                    auto upChunk = ByteChunk::MakeUnique(uPakSize, x_auPakAligns[byPakId]);
                    auto upSeg = qAsm.PopHead();
                    upChunk->Write(upSeg->GetReader(), upSeg->GetReadable());
                    while (upSeg->ucFrag) {
                        upSeg = qAsm.PopHead();
                        upChunk->Write(upSeg->GetReader(), upSeg->GetReadable());
                    }
                    UCP_DBGOUT("pak.id = ", (int) byPakId, ", pak.size = ", uPakSize);
                    x_vUpper.OnPacket(byPakId, ByteBuffer(std::move(upChunk)));
                }
                else {
                    qAsm.PopHead();
                    UCP_DBGOUT("pak.id = ", (int) byPakId, ", pak.size = 0");
                    x_vUpper.OnPacket(byPakId, ByteBuffer());
                }
            }
        }

        inline void X_Flush() {
            UCP_DBGOUT("");
            X_PrepareSaks();
            X_PrepareQSnd();
            bool bFastResend = false;
            bool bTimedOut = false;
            x_usTimeout = StampInfinite(x_usNow);
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (!pSeg->ucSent) {
                    // first time
                    X_EncodeSegment(pSeg);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    bTimedOut = true;
                    if (x_bEchoed)
                        pSeg->ucTimedOut = 0;
                    else if (++pSeg->ucTimedOut > x_kucConnLost)
                        throw ExnIllegalState {};
                    X_EncodeSegment(pSeg);
                }/*
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
                    bFastResend = true;
                    X_EncodeSegment(pSeg);
                }*/
                if (StampBefore(pSeg->usTimeout, x_usTimeout))
                    x_usTimeout = pSeg->usTimeout;
            }
            X_EncodeSaks();
            // flush remaining
            if (x_upFrameSnd->IsReadable())
                X_PostWrite();
            else
                x_upFrameSnd->ToBegin();
            // update cwnd and ssthresh according to RFC5681
            if (bTimedOut) {
                x_uzSsthresh = std::max(x_kuzSsthreshMin, x_uzCwnd >> 1);
                x_uzCwnd = x_kuzCwndMin;
            }/*
            else if (bFastResend) {
                x_uzSsthresh = std::max(x_kuzSsthreshMin, (x_unSndSeq - x_unSndAck) >> 1);
                x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuMss;
            }*/
            UCP_DBGOUT(
                "ssthresh = ", x_uzSsthresh, ", cwnd = ", x_uzCwnd,
                ", now = ", x_usNow, ", next_timeout = ", x_usTimeout
            );
        }

        inline void X_PrepareSaks() noexcept {
            std::sort(x_vecSndSaks.begin(), x_vecSndSaks.end(), std::greater<U32> {});
            USize ucSaks = 0;
            for (auto unSak : x_vecSndSaks)
                if (SeqBefore(x_unRcvSeq, unSak) && (!ucSaks || unSak != x_vecSndSaks[ucSaks - 1]))
                    x_vecSndSaks[ucSaks++] = unSak;
            x_vecSndSaks.resize(ucSaks);
            UCP_DBGOUT("saks = ", x_vecSndSaks);
        }

        inline void X_PrepareQSnd() noexcept {
            auto uzBound = (x_qSnd.IsEmpty() ? x_uzSndIdx : x_qSnd.GetHead()->uzIdx) + x_uzCwnd;
            while (!x_qPak.IsEmpty() && x_uzSndIdx + x_qPak.GetHead()->GetSize() <= uzBound) {
                auto upSeg = x_qPak.PopHead();
                upSeg->unSeq = std::exchange(x_unSndSeq, SeqIncrease(x_unSndSeq));
                upSeg->uzData = static_cast<U16>(upSeg->GetReadable());
                upSeg->uzIdx = x_uzSndIdx;
                upSeg->ucSent = 0;
                upSeg->ucTimedOut = 0;
                x_uzSndIdx += upSeg->GetSize();
                x_qSnd.PushTail(std::move(upSeg));
            }
            UCP_DBGOUT(
                "qsnd.begin = ", uzBound - x_uzCwnd, ", qsnd.end = ", x_uzSndIdx,
                ", bound = ", uzBound
            );
        }

        inline void X_EncodeSegment(SegPayload *pSeg) noexcept {
            x_bNeedAck = false;
            assert(x_upFrameSnd);
            pSeg->unAck = x_unRcvSeq;
            ++pSeg->ucSent;
            pSeg->ucSkipped = 0;
            pSeg->usSent = x_usNow;
            pSeg->usTimeout = x_usNow + x_utRto;
            UCP_DBGOUT("seq = ", pSeg->unSeq, ", ack = ", pSeg->unAck,
                ", timedout = ", pSeg->ucTimedOut, ", timeout = ", pSeg->usTimeout
            );
            if (x_upFrameSnd->GetWritable() < pSeg->GetSize())
                X_PostWrite();
            if (!x_vecSndSaks.empty()) {
                auto ucSaksAppendable = static_cast<U32>(x_upFrameSnd->GetWritable() - pSeg->GetSize()) / 3U;
                auto ucSaksToSend = std::min(ucSaksAppendable, static_cast<U32>(x_vecSndSaks.size()));
                assert(ucSaksToSend < (1U << 12));
                pSeg->ucSaks = ucSaksToSend;
                pSeg->Encode(x_upFrameSnd.get());
                while (ucSaksToSend--) {
                    x_upFrameSnd->Write(&x_vecSndSaks.back(), 3);
                    x_vecSndSaks.pop_back();
                }
            }
            else {
                pSeg->ucSaks = 0;
                pSeg->Encode(x_upFrameSnd.get());
            }
            x_upFrameSnd->Write(pSeg->GetReader(), pSeg->GetReadable());
            // no discard here, since resend is possible
        }

        inline void X_EncodeSaks() noexcept {
            if (!x_vecSndSaks.empty()) {
                x_bNeedAck = false;
                while (!x_vecSndSaks.empty()) {
                    if (x_upFrameSnd->GetWritable() < kuShs + 3)
                        X_PostWrite();
                    auto uSaksToSend = static_cast<U32>(std::min((kuMss - kuShs) / 3, x_vecSndSaks.size()));
                    SegHdr {0, x_unRcvSeq, 0, uSaksToSend, 0, 0}.Encode(x_upFrameSnd.get());
                    UCP_DBGOUT("saks = ", uSaksToSend);
                    while (uSaksToSend--) {
                        x_upFrameSnd->Write(&x_vecSndSaks.back(), 3);
                        x_vecSndSaks.pop_back();
                    }
                }
            }
            else if (x_bNeedAck) {
                x_bNeedAck = false;
                SegHdr vHdr {0, x_unRcvSeq, 0, 0, 0, 0};
                if (x_upFrameSnd->GetWritable() < kuShs)
                    X_PostWrite();
                vHdr.Encode(x_upFrameSnd.get());
                UCP_DBGOUT("ack = ", x_unRcvSeq);
            }
        }

    private:
        inline void X_PostRead() noexcept {
            x_upFrameRcv->ToBegin();
            try {
                x_vLower.PostRead(x_upFrameRcv.get());
            }
            catch (ExnSockRead &) {
                // just let the RDT handle this
            }
            catch (ExnIllegalState) {
                // active shutdown
            }
        }

        inline void X_PostWrite() noexcept {
            try {
                x_vLower.Write(std::move(x_upFrameSnd));
            }
            catch (ExnSockWrite &) {
                // just let the RDT handle this
            }
            catch (ExnIllegalState) {
                // active shutdown
            }
            assert(!x_upFrameSnd);
            x_upFrameSnd = ByteChunk::MakeUnique(kuMss);
        }

    private:
        Upper &x_vUpper;
        Lower x_vLower;
        
        const USize x_auPakAligns[kbyPakIds];

    private:
        RecursiveMutex x_mtx;
        IoGroup *x_pIoGroup = nullptr;
        bool x_bStopping = false;

        std::unique_ptr<ByteChunk> x_upFrameRcv = ByteChunk::MakeUnique(kuMss);
        std::unique_ptr<ByteChunk> x_upFrameSnd = ByteChunk::MakeUnique(kuMss);

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

        U32 x_uzSsthresh = 64 * kuMss; // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;  // cwnd in RFC5681
        U32 x_uzRwnd = 256 * kuMss;

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        SegQue x_qPak {}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {}; // segments sent waiting for acknowledgement
        SegQue x_qRcv {}; // segments received but not in order
        SegQue x_qAsm {}; // segments received and ordered, waiting to be reassemble to packets

        U64 x_uzSndIdx = 0; // index of the byte at the end of x_qSnd increased by 1

        std::vector<U32> x_vecSndSaks; // saks to be sent

    private:
        constexpr static U64 x_kutRtoMin = 400 * 1000;
        constexpr static U64 x_kutRtoMax = 4000 * 1000;
        constexpr static U64 x_kutTick = 10 * 1000;
        constexpr static U32 x_kucFastResend = 3;
        constexpr static U32 x_kucConnLost = 5;
        constexpr static U32 x_kuzSsthreshMin = 2 * kuMss;
        constexpr static U32 x_kuzCwndMin = 1 * kuMss;
        constexpr static U32 x_kuzCwndInit = 3 * kuMss;

    };

}

using ImplUcp::Ucp;
