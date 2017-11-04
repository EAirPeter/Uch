#include "Common.hpp"

int wmain() {
    System::GlobalStartup();
    auto pszCmdLine = GetCommandLineW();
    if (swscanf_s(pszCmdLine, L"%*s%s", g_szWideBuf, static_cast<unsigned>(STRCVT_BUFSIZE)) != 1)
        throw ExnIllegalArg {};
    if (!lstrcmpW(g_szWideBuf, L"+"))
        return RecvMain(pszCmdLine);
    if (!lstrcmpW(g_szWideBuf, L"-"))
        return SendMain(pszCmdLine);
    throw ExnIllegalArg {};
}
