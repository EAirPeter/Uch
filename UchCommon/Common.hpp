#pragma once

#if !defined(NDEBUG) && !defined(_DEBUG)
#define NDEBUG
#endif

#pragma comment(lib, "ws2_32.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <intrin.h>
#include <malloc.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define CONCAT_(a_, b_) a_##b_
#define CONCAT(a_, b_) CONCAT_(a_, b_)

using Byte = unsigned char;
using USize = std::size_t;
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

template<class tElem, USize kuLen>
constexpr USize ArrayLen(tElem (&)[kuLen]) {
    return kuLen;
}

USize GetProcessors() noexcept;

U64 GetTimeStamp() noexcept;

SOCKET CreateTcpSocket();
SOCKET CreateUdpSocket();

constexpr static USize STRCVT_BUFSIZE = 65536;

extern thread_local char g_szUtf8Buf[STRCVT_BUFSIZE];
extern thread_local wchar_t g_szWideBuf[STRCVT_BUFSIZE];

U16 ConvertUtf8ToWide(const char *pszUtf8, int nLen = -1);
U16 ConvertWideToUtf8(const wchar_t *pszWide, int nLen = -1);
U16 ConvertWideToUtf8(const String &sWide);

inline U16 ConvertUtf8ToWide(int nLen = -1) {
    return ConvertUtf8ToWide(g_szUtf8Buf, nLen);
}

inline U16 ConvertWideToUtf8(int nLen = -1) {
    return ConvertWideToUtf8(g_szWideBuf, nLen);
}

U32 CalcCrc32(const U64 *pData, USize uLen) noexcept;

class Bootstrap {
public:
    static Bootstrap &Instance();

private:
    Bootstrap();
    ~Bootstrap();

};
