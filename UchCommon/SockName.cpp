#include "Common.hpp"

#include "SockName.hpp"

SockName GetLocalSockName(SOCKET hSocket) {
    SockName vSockName;
    auto nRes = getsockname(hSocket, &vSockName.vSockAddr, &vSockName.nSockLen);
    if (nRes)
        throw ExnWsa {nRes};
    return vSockName;
}

SockName GetRemoteSockName(SOCKET hSocket) {
    SockName vSockName;
    auto nRes = getpeername(hSocket, &vSockName.vSockAddr, &vSockName.nSockLen);
    if (nRes)
        throw ExnWsa {nRes};
    return vSockName;
}

SockName MakeSockName(const String &sHost, const String &sPort) {
    SockName vSockName;
    ADDRINFOW aih {};
    aih.ai_family = AF_INET;
    ADDRINFOW *pai;
    auto nRes = GetAddrInfoW(sHost.c_str(), sPort.c_str(), &aih, &pai);
    if (nRes)
        throw ExnWsa {nRes};
    std::memcpy(&vSockName.vSockAddr, pai->ai_addr, pai->ai_addrlen);
    vSockName.nSockLen = static_cast<socklen_t>(pai->ai_addrlen);
    FreeAddrInfoW(pai);
    return vSockName;
}

std::pair<String, String> ExtractSockName(const SockName &vSockName) {
    thread_local static wchar_t szHost[NI_MAXHOST];
    thread_local static wchar_t szPort[NI_MAXSERV];
    auto nRes = GetNameInfoW(
        &vSockName.vSockAddr, vSockName.nSockLen,
        szHost, NI_MAXHOST,
        szPort, NI_MAXSERV,
        NI_NUMERICHOST | NI_NUMERICSERV
    );
    if (nRes)
        throw ExnWsa {nRes};
    return {String {szHost}, String {szPort}};
}

String SockNameToString(const SockName &vSockName) {
    auto vRes = ExtractSockName(vSockName);
    return String {L'['} + vRes.first + L':' + vRes.second + L']';
}
