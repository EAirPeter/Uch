#include "Common.hpp"

USize GetProcessors() noexcept {
    SYSTEM_INFO vSysInfo;
    GetSystemInfo(&vSysInfo);
    return vSysInfo.dwNumberOfProcessors;
}

static LARGE_INTEGER f_vQpcFreq;

U64 GetTimeStamp() noexcept {
    LARGE_INTEGER vStamp;
    QueryPerformanceCounter(&vStamp);
    return static_cast<U64>(vStamp.QuadPart) * 1000000ULL / static_cast<U64>(f_vQpcFreq.QuadPart);
}

SOCKET CreateTcpSocket() {
    auto sock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET)
        throw ExnWsa();
    return sock;
}

SOCKET CreateUdpSocket() {
    SOCKET sock = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET)
        throw ExnWsa();
    return sock;
}

HANDLE CreateFileHandle(const String &sPath, DWORD dwAccess, DWORD dwCreation, DWORD dwFlags) {
    auto hFile = CreateFileW(
        sPath.c_str(), dwAccess, 0, nullptr, dwCreation, dwFlags | FILE_FLAG_OVERLAPPED, nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
        throw ExnSys();
    return hFile;
}
thread_local char g_szUtf8Buf[STRCVT_BUFSIZE];
thread_local wchar_t g_szWideBuf[STRCVT_BUFSIZE];

U16 ConvertUtf8ToWide(const char *pszUtf8, int nLen) {
    if (!nLen) {
        g_szWideBuf[0] = L'\0';
        return 0;
    }
    auto nRes = MultiByteToWideChar(CP_UTF8, 0, pszUtf8, nLen, g_szWideBuf, STRCVT_BUFSIZE);
    if (!nRes)
        throw ExnSys();
    return static_cast<U16>(nRes);
}

U16 ConvertWideToUtf8(const wchar_t *pszWide, int nLen) {
    if (!nLen) {
        g_szUtf8Buf[0] = '\0';
        return 0;
    }
    auto nRes = WideCharToMultiByte(CP_UTF8, 0, pszWide, nLen, g_szUtf8Buf, STRCVT_BUFSIZE, nullptr, nullptr);
    if (!nRes)
        throw ExnSys();
    return static_cast<U16>(nRes);
}

U16 ConvertWideToUtf8(const String & sWide) {
    if (sWide.size() != static_cast<U16>(sWide.size()))
        throw ExnArgTooLarge {sWide.size(), 65535};
    return ConvertWideToUtf8(sWide.c_str(), static_cast<int>(sWide.size()));
}

/*
U32 CalcCrc32(const U64 *pData, USize uLen) noexcept {
    U64 uCrc = 0;
    while (uLen--)
        uCrc = _mm_crc32_u64(uCrc, pData[uLen]);
    return static_cast<U32>(uCrc);
}
*/

Bootstrap &Bootstrap::Instance() {
    static Bootstrap instance;
    return instance;
}

Bootstrap::Bootstrap() {
    WSADATA wsa;
    auto nRes = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (nRes)
        throw ExnWsa(nRes);
    auto hSock = CreateUdpSocket();
    GUID guidTransmitPackets = WSAID_TRANSMITPACKETS;
    DWORD dwDone;
    nRes = WSAIoctl(
        hSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidTransmitPackets, static_cast<DWORD>(sizeof(guidTransmitPackets)),
        &Wsimp::TransmitPackets, static_cast<DWORD>(sizeof(Wsimp::TransmitPackets)),
        &dwDone, nullptr, nullptr
    );
    if (nRes) {
        nRes = WSAGetLastError();
        WSACleanup();
        throw ExnWsa(nRes);
    }
    QueryPerformanceFrequency(&f_vQpcFreq);
}

Bootstrap::~Bootstrap() {
    WSACleanup();
}

namespace Wsimp {
    LPFN_TRANSMITPACKETS TransmitPackets;

}
