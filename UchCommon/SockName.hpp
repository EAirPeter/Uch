#pragma once

#include "Common.hpp"

struct SockName {
    sockaddr vSockAddr {};
    socklen_t nSockLen {sizeof(sockaddr)};
};

SockName GetLocalSockName(SOCKET hSocket);

SockName GetRemoteSockName(SOCKET hSocket);

SockName MakeSockName(const String &sHost, const String &sPort);

std::pair<String, String> ExtractSockName(const SockName &vSockName);

String SockNameToString(const SockName &vSockName);
