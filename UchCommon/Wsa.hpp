#pragma once

#include "Common.hpp"

struct SockName {
    sockaddr vSockAddr {};
    socklen_t nSockLen {sizeof(sockaddr)};

    operator String() const;

    constexpr U16 GetPort() const noexcept {
        return reinterpret_cast<const sockaddr_in *>(&vSockAddr)->sin_port;
    }

    constexpr void SetPort(U16 uPort) noexcept {
        reinterpret_cast<sockaddr_in *>(&vSockAddr)->sin_port = uPort;
    }

};

BUFOPR(SockName)

SockName GetLocalSockName(SOCKET hSocket);
SockName GetRemoteSockName(SOCKET hSocket);
SockName MakeSockName(const String &sHost, const String &sPort);
std::pair<String, String> ExtractSockName(const SockName &vSockName);
String SockNameToString(const SockName &vSockName);

SOCKET CreateSocket(bool bTcp = true);
SOCKET CreateConnectedSocket(const SockName &vSockName, bool bTcp = true);
SOCKET CreateBoundSocket(const SockName &vSockName, bool bTcp = true);

namespace wsaimp {
    extern LPFN_TRANSMITPACKETS TransmitPackets;
}
