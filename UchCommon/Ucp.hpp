#pragma once

#include "Common.hpp"

#include "ByteBuffer.hpp"
#include "ByteChunk.hpp"
#include "IntrList.hpp"
#include "SockIo.hpp"
#include "StaticChunk.hpp"
#include "Sync.hpp"

namespace ImplUcp {    
    //  0                               1
    //  0 1 2 3 4 5 6 7 8 9 A B C D E F 0 1 2 3 4 5 6 7 8 9 A B C D E F
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // | sequence number                                         unSeq |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // | acknowledgement number                                  unAck |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // | number of saks          ucSak | size of payload        uzData |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // | selective acknowledgements                             unSaks |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // | payload                                               abyData |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

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

    struct SegHdr {
        inline SegHdr() noexcept = default;
        constexpr SegHdr(const SegHdr &) noexcept = default;

        U32 unSeq;
        U32 unAck;
        U16 ucSak;
        U16 uzData;

    };

    static_assert(offsetof(SegHdr, unSeq) == 0, "seghdr");
    static_assert(offsetof(SegHdr, unAck) == 4, "seghdr");
    static_assert(offsetof(SegHdr, ucSak) == 8, "seghdr");
    static_assert(offsetof(SegHdr, uzData) == 10, "seghdr");
    static_assert(sizeof(SegHdr) == 12, "seghdr");

    constexpr static USize kuMss = 1400;
    constexpr static USize kuSzHdr = sizeof(SegHdr);
    constexpr static USize kuMps = kuMss - kuSzHdr;

    struct SegPayload : SegHdr, StaticChunk<kuMps>, IntrListNode<SegPayload> {
        inline SegPayload() noexcept = default;
        inline SegPayload(const SegHdr &vHdr) noexcept : SegHdr(vHdr), StaticChunk(), IntrListNode() {}

        constexpr USize GetSize() noexcept {
            return sizeof(SegHdr) + StaticChunk::GetReadable();
        }

        USize uzIdx;        // index of the first byte
        U32 ucSent = 0;     // sent count
        U32 ucSkipped;      // skipped by sak count
        U64 usSent;         // when the segment is sent
        U64 usTimeout;      // when the segment should be resent due to timed out

    };

    template<class tUpper, Byte kbyPakIds>
    class Ucp {
    private:
        using Upper = tUpper;
        using Lower = SockIo<Ucp>;
        using SegQue = IntrList<SegPayload>;

    public:
        Ucp(const Ucp &) = delete;
        Ucp(Ucp &&) = delete;

        template<USize ...kuPakSizes, USize ...kuPakAligns>
        Ucp(
            Upper &vUpper, SOCKET hSocket,
            std::index_sequence<kuPakSizes...>,
            std::index_sequence<kuPakAligns...>
        ) :
            x_vUpper(vUpper), x_vLower(*this, hSocket),
            x_auPakSizes {kuPakSizes...},
            x_auPakAligns {kuPakAligns...}
        {
            static_assert(kbyPakIds == sizeof...(kuPakSizes), "sizes");
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
            if (!x_bDirty)
                x_vLower.Shutdown();
        }

        inline void Close() noexcept {
            RAII_LOCK(x_mtx);
            x_bStopping = true;
            x_pIoGroup->UnregisterTick(X_OnTick, this);
            x_vLower.Close();
        }

        void OnFinalize() noexcept {
            RAII_LOCK(x_mtx);
            x_bStopping = true;
            x_vUpper.OnFinalize();
        }

        inline void OnRead(DWORD dwError, USize uDone, ByteChunk *pChunk) {
            RAII_LOCK(x_mtx);
            if (x_bBroken)
                return;
            if (dwError) {
                // just let the RDT handle this
                // X_OnError(static_cast<int>(dwError));
                return;
            }
            assert(uDone == pChunk->GetReadable());
            assert(pChunk == x_upFrameRcv.get());
            x_bDirty = true;
            X_DecodeFrame();
            X_PostRead();
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
                return true;
            }
            x_usNow = usNow;
            if (x_bDirty || StampDue(x_usNow, x_usTimeout)) {
                x_bDirty = false;
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
            }
            return true;
        }

    public:
        inline void PostPacket(Byte byPakId, ByteBuffer &vPakBuf) {
            if (byPakId >= kbyPakIds)
                throw ExnIllegalArg {};
            if (vPakBuf.GetSize() > x_auPakSizes[byPakId])
                throw ExnIllegalArg {};
            RAII_LOCK(x_mtx);
            if (x_bStopping)
                throw ExnIllegalState {};
            x_bDirty = true;
            X_QueuePacket(byPakId, vPakBuf);
        }

        inline void PostPacket(Byte byPakId, ByteBuffer &&vPakBuf) {
            PostPacket(byPakId, vPakBuf);
        }

        // OnPacket(Byte byPakId, ByteBuffer vPakBuf);

    private:
        void X_QueuePacket(Byte byPakId, ByteBuffer &vPakBuf) noexcept {
            auto pTail = x_qPak.GetTail();
            if (pTail == x_qPak.GetNil() || !pTail->IsWritable()) {
                x_qPak.PushTail(std::make_unique<SegPayload>());
                pTail = x_qPak.GetTail();
                auto pPrev = pTail->GetPrev();
                pTail->uzIdx = pPrev == x_qPak.GetNil() ? x_uzSndIdx : pPrev->uzIdx + pPrev->GetSize();
            }
            pTail->Write(&byPakId, 1);
            auto uToPad = x_auPakSizes[byPakId] - vPakBuf.GetSize();
            while (!vPakBuf.IsEmpty()) {
                auto upChunk = vPakBuf.PopChunk();
                while (upChunk->IsReadable()) {
                    if (!pTail->IsWritable()) {
                        x_qPak.PushTail(std::make_unique<SegPayload>());
                        pTail = x_qPak.GetTail();
                    }
                    auto uToWrite = std::min(pTail->GetWritable(), upChunk->GetReadable());
                    pTail->Write(upChunk->GetReader(), uToWrite);
                    upChunk->Discard(uToWrite);
                }
            }
            while (uToPad) {
                if (!pTail->IsWritable()) {
                    x_qPak.PushTail(std::make_unique<SegPayload>());
                    pTail = x_qPak.GetTail();
                    auto pPrev = pTail->GetPrev();
                    pTail->uzIdx = pPrev == x_qPak.GetNil() ? x_uzSndIdx : pPrev->uzIdx + pPrev->GetSize();
                }
                auto uToFill = std::min(pTail->GetWritable(), uToPad);
                pTail->Fill(0, uToFill);
                uToPad -= uToFill;
            }
        }

        void X_DecodeFrame() noexcept {
            U32 unOldSndAck = x_unSndAck;
            U32 unSakMax = 0;
            U32 uzAcked = 0;
            while (x_upFrameRcv->IsReadable()) {
                SegHdr vHdr;
                assert(x_upFrameRcv->GetReadable() >= sizeof(SegHdr));
                x_upFrameRcv->Read(&vHdr, sizeof(SegHdr));
                X_OnAck(uzAcked, vHdr.unAck);
                while (vHdr.ucSak--) {
                    U32 uSak;
                    assert(x_upFrameRcv->GetReadable() >= sizeof(U32));
                    x_upFrameRcv->Read(&uSak, sizeof(U32));
                    X_OnSak(uzAcked, uSak);
                    unSakMax = std::max(unSakMax, uSak);
                }
                if (vHdr.uzData)
                    X_OnPsh(vHdr);
            }
            assert(!x_upFrameRcv->IsReadable());
            X_UpdateQSnd(unSakMax);
            X_UpdateQRcv();
            X_UpdateQAsm();
            // update cwnd and ssthresh according to RFC5681
            if (x_unSndAck > unOldSndAck) {
                if (x_uzCwnd < x_uzSsthresh)
                    x_uzCwnd += std::min(uzAcked, static_cast<U32>(kuMss));
                else
                    x_uzCwnd += std::min(static_cast<U32>(kuMss * kuMss / x_uzCwnd), static_cast<U32>(1));
            }
        }

        inline void X_OnAck(U32 &uzAcked, U32 unAck) noexcept {
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && pSeg->unSeq < unAck) {
                if (pSeg->ucSent == 1)
                    X_UpdateRtt(x_usNow - pSeg->usSent);
                uzAcked += static_cast<U32>(pSeg->GetSize());
                pSeg = pSeg->Remove()->GetNext();
            }
        }

        inline void X_OnSak(U32 &uzAcked, U32 unSak) noexcept {
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && pSeg->unSeq < unSak)
                pSeg = pSeg->GetNext();
            if (pSeg != x_qSnd.GetNil() && pSeg->unSeq == unSak) {
                if (pSeg->ucSent == 1)
                    X_UpdateRtt(x_usNow - pSeg->usSent);
                uzAcked += static_cast<U32>(pSeg->GetSize());
                pSeg->Remove();
            }
        }

        inline void X_OnPsh(const SegHdr &vHdr) noexcept {
            x_bNeedAck = true;
            x_vecSndSaks.emplace_back(vHdr.unSeq);
            assert(x_upFrameRcv->GetReadable() >= vHdr.uzData);
            if (vHdr.unSeq < x_unRcvSeq) {
                // ack may lost, resend ack on next tick
                x_upFrameRcv->Discard(vHdr.uzData);
                return;
            }
            auto pSeg = x_qRcv.GetTail();
            while (pSeg != x_qRcv.GetNil() && pSeg->unSeq > vHdr.unSeq)
                pSeg = pSeg->GetPrev();
            if (pSeg != x_qRcv.GetNil() && pSeg->unSeq == vHdr.unSeq) {
                // duplicated segment, ack may lost, resend ack on next tick
                x_upFrameRcv->Discard(vHdr.uzData);
                return;
            }
            auto upSeg = std::make_unique<SegPayload>(std::move(vHdr));
            assert(x_upFrameRcv->GetReadable() <= upSeg->GetWritable());
            x_upFrameRcv->Read(upSeg->GetWriter(), vHdr.uzData);
            upSeg->Skip(vHdr.uzData);
            // payload retrieved, insert to x_qRcv
            pSeg->InsertAfter(std::move(upSeg));
        }

        inline void X_UpdateRtt(U64 utRtt) noexcept {
            if (static_cast<I64>(utRtt) <= 0) // something happened
                return;
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
            x_utRto = x_utSRtt + std::max(x_kutTick, x_utRttVar << 2);
        }

        inline void X_UpdateQSnd(U32 unSakMax) noexcept {
            x_unSndAck = x_qSnd.IsEmpty() ? x_unSndSeq : x_qSnd.GetHead()->unSeq;
            if (unSakMax <= x_unSndAck)
                return;
            auto pSeg = x_qSnd.GetHead();
            while (pSeg != x_qSnd.GetNil() && pSeg->unSeq < unSakMax) {
                ++pSeg->ucSkipped;
                pSeg = pSeg->GetNext();
            }
        }

        inline void X_UpdateQRcv() noexcept {
            auto pSeg = x_qRcv.GetHead();
            while (pSeg != x_qRcv.GetNil() && pSeg->unSeq == x_unRcvSeq) {
                x_qAsm.PushTail(pSeg->Remove());
                assert(x_qAsm.GetTail()->GetReadable() == x_qAsm.GetTail()->uzData);
                x_uzAsm += x_qAsm.GetTail()->uzData;
                ++x_unRcvSeq;
                pSeg = x_qRcv.GetHead();
            }
        }

        inline void X_UpdateQAsm() noexcept {
            auto pHead = x_qAsm.GetHead();
            if (x_byPakId == 0xff && x_uzAsm) {
                pHead->Read(&x_byPakId, 1);
                --x_uzAsm;
                assert(x_byPakId != 0xff);
                if (!pHead->IsReadable())
                    pHead = pHead->Remove()->GetNext();
            }
            while (x_byPakId != 0xff && x_uzAsm >= x_auPakSizes[x_byPakId]) {
                const auto uPakSize = x_auPakSizes[x_byPakId];
                if (uPakSize) {
                    auto upChunk = ByteChunk::MakeUnique(uPakSize, x_auPakAligns[x_byPakId]);
                    auto uSize = uPakSize;
                    while (uSize) {
                        auto uToRead = std::min(uSize, pHead->GetReadable());
                        pHead->Read(upChunk->GetWriter(), uToRead);
                        upChunk->Skip(uToRead);
                        uSize -= uToRead;
                        if (!pHead->IsReadable())
                            pHead = pHead->Remove()->GetNext();
                    }
                    x_uzAsm -= uPakSize;
                    ByteBuffer vPakBuf(std::move(upChunk));
                    x_vUpper.OnPacket(x_byPakId, std::move(vPakBuf));
                }
                else
                    x_vUpper.OnPacket(x_byPakId, ByteBuffer());
                if (x_uzAsm) {
                    pHead->Read(&x_byPakId, 1);
                    --x_uzAsm;
                    assert(x_byPakId != 0xff);
                    if (!pHead->IsReadable())
                        pHead = pHead->Remove()->GetNext();
                }
                else
                    x_byPakId = 0xff;
            }
        }

        inline void X_Flush() {
            X_PrepareSaks();
            X_PrepareQSnd();
            bool bFastResend = false;
            bool bTimedOut = false;
            x_usTimeout = x_usNow + 0x8000000000000000ULL;
            for (auto pSeg = x_qSnd.GetHead(); pSeg != x_qSnd.GetNil(); pSeg = pSeg->GetNext()) {
                if (!pSeg->ucSent) {
                    // first time
                    X_EncodeSegment(pSeg);
                }
                else if (StampDue(x_usNow, pSeg->usTimeout)) {
                    // timed out
                    bTimedOut = true;
                    X_EncodeSegment(pSeg);
                }
                else if (pSeg->ucSkipped >= x_kucFastResend) {
                    // fast resend
                    bFastResend = true;
                    X_EncodeSegment(pSeg);
                }
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
            }
            else if (bFastResend) {
                x_uzSsthresh = std::max(x_kuzSsthreshMin, (x_unSndSeq - x_unSndAck) >> 1);
                x_uzCwnd = x_uzSsthresh + x_kucFastResend * kuMss;
            }
        }

        inline void X_PrepareSaks() noexcept {
            std::sort(x_vecSndSaks.begin(), x_vecSndSaks.end());
            USize ucSaks = 0;
            for (auto unSak : x_vecSndSaks)
                if (unSak > x_unRcvSeq && (!ucSaks || unSak != x_vecSndSaks[ucSaks - 1]))
                    x_vecSndSaks[ucSaks++] = unSak;
            x_vecSndSaks.resize(ucSaks);
        }

        inline void X_PrepareQSnd() noexcept {
            auto uzBound = (x_qSnd.IsEmpty() ? x_uzSndIdx : x_qSnd.GetHead()->uzIdx) + x_uzCwnd;
            while (!x_qPak.IsEmpty() && x_uzSndIdx + x_qPak.GetHead()->GetSize() <= uzBound) {
                auto upSeg = x_qPak.PopHead();
                upSeg->unSeq = x_unSndSeq++;
                upSeg->uzData = static_cast<U16>(upSeg->GetReadable());
                upSeg->uzIdx = x_uzSndIdx;
                x_uzSndIdx += upSeg->GetSize();
                x_qSnd.PushTail(std::move(upSeg));
            }
        }

        inline void X_EncodeSegment(SegPayload *pSeg) {
            x_bNeedAck = false;
            assert(x_upFrameSnd);
            pSeg->unAck = x_unRcvSeq;
            ++pSeg->ucSent;
            if (pSeg->ucSent > x_kucConnLost)
                throw ExnIllegalState();
            pSeg->ucSkipped = 0;
            pSeg->usSent = x_usNow;
            pSeg->usTimeout = x_usNow + x_utRto;
            if (StampBefore(pSeg->usTimeout, x_usTimeout))
                x_usTimeout = pSeg->usTimeout;
            if (x_upFrameSnd->GetWritable() < pSeg->GetSize())
                X_PostWrite();
            auto ucSakAppendable = (x_upFrameSnd->GetWritable() - pSeg->GetSize()) / sizeof(U32);
            auto ucSakToSend = std::min(ucSakAppendable, x_vecSndSaks.size() - x_ucSakSent);
            pSeg->ucSak = static_cast<U16>(ucSakToSend);
            assert(pSeg->ucSak == ucSakToSend);
            x_upFrameSnd->Write(static_cast<SegHdr *>(pSeg), sizeof(SegHdr));
            if (ucSakToSend) {
                x_upFrameSnd->Write(x_vecSndSaks.data() + x_ucSakSent, sizeof(U32) * ucSakToSend);
                x_ucSakSent += ucSakToSend;
            }
            x_upFrameSnd->Write(pSeg->GetReader(), pSeg->GetReadable());
            // no discard here, since resend is possible
        }

        inline void X_EncodeSaks() noexcept {
            if (x_bNeedAck || x_ucSakSent != x_vecSndSaks.size()) {
                x_bNeedAck = false;
                auto uSakToSend = x_vecSndSaks.size() - x_ucSakSent;
                SegHdr vHdr {0, x_unRcvSeq, static_cast<U16>(uSakToSend), 0};
                assert(uSakToSend == vHdr.ucSak);
                if (x_upFrameSnd->GetWritable() < sizeof(SegHdr) + sizeof(U32) * uSakToSend)
                    X_PostWrite();
                x_upFrameSnd->Write(&vHdr, sizeof(SegHdr));
                if (uSakToSend)
                    x_upFrameSnd->Write(x_vecSndSaks.data() + x_ucSakSent, sizeof(U32) * uSakToSend);
            }
            x_vecSndSaks.clear();
            x_ucSakSent = 0;
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

        const USize x_auPakSizes[kbyPakIds];
        const USize x_auPakAligns[kbyPakIds];

    private:
        RecursiveMutex x_mtx;
        IoGroup *x_pIoGroup = nullptr;
        bool x_bStopping = false;

        std::unique_ptr<ByteChunk> x_upFrameRcv = ByteChunk::MakeUnique(kuMss);
        std::unique_ptr<ByteChunk> x_upFrameSnd = ByteChunk::MakeUnique(kuMss);
        Byte x_byPakId = 0xff;

    private:
        bool x_bBroken = false;  // whether Ucp is closed due to timed out
        bool x_bDirty = false;   // whether flush is needed
        bool x_bNeedAck = false; // whether an empty ack segment is needed
        U64 x_usNow = 0;         // last tick time
        U64 x_usTimeout = 0;     // next time for resend

        U64 x_utRttVar = 0;        // RTTVAR in RFC6298
        U64 x_utSRtt = 0;          // SRTT in RFC6298
        U64 x_utRto = x_kutRtoMax; // RTO in RFC6298

        U32 x_uzSsthresh = 64 * kuMss; // ssthresh in RFC5681
        U32 x_uzCwnd = x_kuzCwndInit;  // cwnd in RFC5681

        U32 x_unSndSeq = 0; // unSeq of the next segment to be put in x_qSnd
        U32 x_unSndAck = 0; // unSeq of the first segment of x_qSnd
        U32 x_unRcvSeq = 0; // expected unSeq of the next segment to be received

        SegQue x_qPak {}; // segments waiting to be sent, formed by fragmenting packets posted
        SegQue x_qSnd {}; // segments sent waiting for acknowledgement
        SegQue x_qRcv {}; // segments received but not in order
        SegQue x_qAsm {}; // segments received and ordered, waiting to be reassemble to packets

        USize x_uzSndIdx = 0; // index of the byte at the end of x_qSnd increased by 1
        USize x_uzAsm = 0;    // size of x_qAsm

        std::vector<U32> x_vecSndSaks; // saks to be sent
        USize x_ucSakSent;             // saks sent

    private:
        constexpr static U64 x_kutRtoMin = 400 * 1000;
        constexpr static U64 x_kutRtoMax = 4000 * 1000;
        constexpr static U64 x_kutTick = 10 * 1000;
        constexpr static U32 x_kuwSnd = 256;
        constexpr static U32 x_kucFastResend = 3;
        constexpr static U32 x_kucConnLost = 5;
        constexpr static U32 x_kuzSsthreshMin = 2 * kuMss;
        constexpr static U32 x_kuzCwndMin = 1 * kuMss;
        constexpr static U32 x_kuzCwndInit = 3 * kuMss;

    };

}

using ImplUcp::Ucp;
