#include "Common.hpp"

HANDLE HandleCast(UPtr uHandle) {
    auto hVal = reinterpret_cast<HANDLE>(uHandle);
    DWORD dw;
    if (!GetHandleInformation(reinterpret_cast<HANDLE>(hVal), &dw))
        throw ExnSys();
    return hVal;
}

void PrintConsole(const String &s) {
    static auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    auto uLen = Format(L"[%s] %s", FormattedTime().c_str(), s.c_str());
    WriteConsoleW(hStdout, g_szWideBuf, static_cast<DWORD>(uLen), nullptr, nullptr);
}
