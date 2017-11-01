#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "Flags.hpp"
#include "Pool.hpp"

namespace ImplUcp {
    //  0                   1                   2                   3
    //  0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | unSeq                       | unAck                       | uzData            |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | ucRwnd                      | ubFlags |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    // | (payload, uzData bytes in total) / (saks, uzData * 3 bytes in total)          |
    // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+

    // unSeq is valid if ucSize is not 0
    // if ucSize is 0, the segment does not need acknowledgement
    // unAck means any segment with unSeq < unAck has been acknowledged for sender
    // any one of unSaks should be greater than unAck

    // ub: bitset
    // uc: number of count
    // un: sequence number, for: seq, ack, sak
    // us: time stamp in microseconds according to QPC
    // ut: time duration in microseconds
    // uw: size of window in count of segments
    // uz: size in bytes
    //  q: segment queue

    constexpr static U32 kuzMtu = 1500;
    constexpr static U32 kuzIpHdr = 20;
    constexpr static U32 kuzUdpHdr = 8;
    constexpr static U32 kuzUcpHdr = 12;
    constexpr static U32 kuzMss = kuzMtu - kuzIpHdr - kuzUdpHdr;
    constexpr static U32 kuzPayload = kuzMss - kuzUcpHdr;
    constexpr static U32 kuMaxSaks = (kuzPayload - 1) / 3;

    constexpr bool SeqBefore(U32 unSub, U32 unObj) noexcept {
        return (unSub - unObj) & 0x01000000;
    }

    constexpr U32 SeqIncrease(U32 unSeq, U32 ucHow = 1) noexcept {
        return (unSeq + ucHow) & 0x00ffffff;
    }

    constexpr U32 SeqDecrease(U32 unSeq, U32 ucHow = 1) noexcept {
        return (unSeq - ucHow) & 0x00ffffff;
    }

    constexpr U32 kubSegFrg = 0x01;
    constexpr U32 kubSegPsh = 0x02;
    constexpr U32 kubSegSak = 0x04;
    constexpr U32 kubSegAsk = 0x08;

    struct UcpSeg : ByteChunk<kuzMss>, IntrListNode<UcpSeg> {
        template<class tUpper>
        friend class Ucp;

    private:
        struct SegRecv {};

    public:
        inline UcpSeg() noexcept : ByteChunk(kuzUcpHdr) {}

        inline UcpSeg(SegRecv) noexcept : ByteChunk() {}

        inline UcpSeg(U32 unSeq_, U32 unAck_, U32 ucRwnd_, U32 uzData_, U32 ubFlags_) noexcept :
            unSeq(unSeq_), unAck(unAck_), ucRwnd(ucRwnd_), uzData(uzData_), vFlags(ubFlags_)
        {
            Encode();
            x_pWriter = &x_abyData[kuzUcpHdr];
        }

        inline void Decode() noexcept {
            assert(GetReadable() >= kuzUcpHdr);
            assert(x_pReader == x_abyData);
            unSeq = *reinterpret_cast<U32 *>(&x_abyData[0]) & 0x00ffffff;
            unAck = *reinterpret_cast<U32 *>(&x_abyData[3]) & 0x00ffffff;
            uzData = *reinterpret_cast<U16 *>(&x_abyData[6]);
            ucRwnd = *reinterpret_cast<U32 *>(&x_abyData[8]) & 0x00ffffff;
            vFlags = *reinterpret_cast<U8 *>(&x_abyData[11]);
            x_pReader = &x_abyData[kuzUcpHdr];
        }

        inline void Encode() noexcept {
            assert(!(unSeq & 0xff000000));
            assert(!(unAck & 0xff000000));
            assert(!(ucRwnd & 0xff000000));
            assert(!(uzData & 0xffff0000));
            assert(!(static_cast<U32>(vFlags) & 0xffffff00));
            *reinterpret_cast<U32 *>(&x_abyData[0]) = unSeq;
            *reinterpret_cast<U32 *>(&x_abyData[3]) = unAck;
            *reinterpret_cast<U16 *>(&x_abyData[6]) = static_cast<U16>(uzData);
            *reinterpret_cast<U32 *>(&x_abyData[8]) = ucRwnd;
            *reinterpret_cast<U8 *>(&x_abyData[11]) = static_cast<U8>(static_cast<U32>(vFlags));
            x_pReader = x_abyData;
        }

        constexpr U32 GetSize() noexcept {
            return kuzUcpHdr + uzData;
        }

    private:
        U32 unSeq;          // 24-bit sequence number
        U32 unAck;          // 24-bit acknowledgement number
        U32 ucRwnd;         // 16-bit size of sender's rwnd in count of mss-s
        U32 uzData;         // 12-bit count of saks or size of payload in bytes
        Flags<U32> vFlags;  // 8-bit fragment number

        U32 ucSent;         // sent count
        U32 ucTimedOut;     // timed out count
        U32 ucSkipped;      // skipped by sak count
        U64 usSent;         // when the segment is sent
        U64 usTimeout;      // when the segment should be resent due to timed out

    };

    using SegPool = Pool<UcpSeg>;
    using SegQue = IntrList<UcpSeg, SegPool>;

    struct UcpBuffer : ByteBuffer<UcpSeg, SegPool, kuzPayload> {
        template<class tUpper>
        friend class Ucp;

        using ByteBuffer::ByteBuffer;

    };

}

constexpr static U32 kuzUcpHeader = ImplUcp::kuzUcpHdr;
constexpr static U32 kuzUcpPayload = ImplUcp::kuzPayload;
constexpr static U32 kuzUcpSegment = ImplUcp::kuzMss;

using ImplUcp::UcpBuffer;
