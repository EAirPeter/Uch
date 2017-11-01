#include "Common.hpp"

#include "String.hpp"
#include "System.hpp"
#include "Wsa.hpp"

static LARGE_INTEGER f_vQpcFreq;

void System::GlobalStartup() {
    static System vInstance {};
}

System::System() {
    WSADATA wsa;
    auto nRes = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (nRes)
        throw ExnWsa(nRes);
    auto hSock = CreateSocket(false);
    GUID guidTransmitPackets = WSAID_TRANSMITPACKETS;
    DWORD dwDone;
    nRes = WSAIoctl(
        hSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidTransmitPackets, static_cast<DWORD>(sizeof(guidTransmitPackets)),
        &wsaimp::TransmitPackets, static_cast<DWORD>(sizeof(wsaimp::TransmitPackets)),
        &dwDone, nullptr, nullptr
    );
    if (nRes) {
        nRes = WSAGetLastError();
        WSACleanup();
        throw ExnWsa(nRes);
    }
    QueryPerformanceFrequency(&f_vQpcFreq);
}

System::~System() {
    WSACleanup();
}

USize GetProcessors() noexcept {
    SYSTEM_INFO vSysInfo;
    GetSystemInfo(&vSysInfo);
    return vSysInfo.dwNumberOfProcessors;
}

HANDLE CreateFileHandle(const String &sPath, DWORD dwAccess, DWORD dwCreation, DWORD dwFlags) {
    auto hFile = CreateFileW(
        sPath.c_str(), dwAccess, 0, nullptr, dwCreation, dwFlags, nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
        throw ExnSys();
    return hFile;
}

U64 GetTimeStamp() noexcept {
    LARGE_INTEGER vStamp;
    QueryPerformanceCounter(&vStamp);
    return static_cast<U64>(vStamp.QuadPart) * 1000000ULL / static_cast<U64>(f_vQpcFreq.QuadPart);
}

String FormattedTime() noexcept {
    auto nRes = GetTimeFormatEx(
        LOCALE_NAME_USER_DEFAULT,
        0, nullptr, L"hh:mm:ss tt",
        g_szWideBuf, static_cast<int>(STRCVT_BUFSIZE)
    );
    assert(nRes);
    return {g_szWideBuf, static_cast<USize>(nRes)};
}
