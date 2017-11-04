#pragma once

#if !defined(NDEBUG) && !defined(_DEBUG)
#define NDEBUG
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#define _WIN32_WINNT 0x0601

#include <sdkddkver.h>

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <MSWSock.h>

#include <malloc.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <numeric>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define CONCAT_(a_, b_) a_##b_
#define CONCAT(a_, b_) CONCAT_(a_, b_)

#define REQUIRES(...) std::enable_if_t<__VA_ARGS__, int> = 0

#define BUFOPR(type_) \
    template<class tBuffer> \
    inline tBuffer &operator >>(tBuffer &vBuf, type_ &vObj) noexcept { \
        vBuf.ReadBytes(&vObj, static_cast<U32>(sizeof(type_))); \
        return vBuf; \
    } \
    template<class tBuffer> \
    inline tBuffer &operator <<(tBuffer &vBuf, const type_ &vObj) noexcept { \
        vBuf.WriteBytes(&vObj, static_cast<U32>(sizeof(type_))); \
        return vBuf; \
    }

using Byte = unsigned char;
using USize = std::size_t;
using UPtr = std::uintptr_t;
using IPtr = std::intptr_t;
using U8 = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;
using I8 = std::int8_t;
using I16 = std::int16_t;
using I32 = std::int32_t;
using I64 = std::int64_t;

using String = std::wstring;

struct ExnIllegalState {};

struct ExnIllegalArg {};

struct ExnNoEnoughData {
    USize uRequested;
    USize uActual;
};

struct ExnArgTooLarge {
    USize uRequested;
    USize uMaximum;
};

struct ExnSys {
    inline ExnSys() : dwError(GetLastError()) {}
    constexpr ExnSys(DWORD dw) : dwError(dw) {}
    DWORD dwError;
};

struct ExnWsa {
    inline ExnWsa() : nError(WSAGetLastError()) {}
    constexpr ExnWsa(int n) : nError(n) {}
    int nError;
};
