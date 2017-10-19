#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "Pool.hpp"
#include "StaticChunk.hpp"

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

    constexpr static U32 kuMss = 1400;
    constexpr static U32 kuShs = 12;
    constexpr static U32 kuMps = kuMss - kuShs;
    constexpr static U32 kuMks = kuMps * 256;

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

    constexpr U32 SeqIncrease(U32 unSeq, U32 ucHow = 1) noexcept {
        return (unSeq + ucHow) & 0x00ffffff;
    }

    constexpr U32 SeqDecrease(U32 unSeq, U32 ucHow = 1) noexcept {
        return (unSeq - ucHow) & 0x00ffffff;
    }

    struct SegPayload : SegHdr, StaticChunk<kuMps>, IntrListNode<SegPayload> {
        static inline void *operator new(USize) noexcept {
            return Pool<SegPayload>::Instance().Alloc();
        }

        static inline void operator delete(void *pSeg) noexcept {
            Pool<SegPayload>::Instance().Dealloc(reinterpret_cast<SegPayload *>(pSeg));
        }

        inline SegPayload() noexcept = default;
        inline SegPayload(const SegHdr &vHdr) noexcept : SegHdr(vHdr), StaticChunk(), IntrListNode() {}
        inline SegPayload(ByteChunk *pChunk) noexcept : SegHdr(pChunk), StaticChunk(), IntrListNode() {}

        constexpr U32 GetSize() noexcept {
            return kuShs + uzData;
        }

        U64 uzIdx;          // index of the first byte
        U32 ucSent;         // sent count
        U32 ucTimedOut;     // timed out count
        U32 ucSkipped;      // skipped by sak count
        U64 usSent;         // when the segment is sent
        U64 usTimeout;      // when the segment should be resent due to timed out

    };

}
