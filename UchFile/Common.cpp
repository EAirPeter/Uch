#pragma once

#include "Common.hpp"

void PrintConsole(const String &s) {
    static auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    auto uLen = Format(L"[%s] %s", FormattedTime().c_str(), s.c_str());
    WriteConsoleW(hStdout, g_szWideBuf, static_cast<DWORD>(uLen), nullptr, nullptr);
}
