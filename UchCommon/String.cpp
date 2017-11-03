#include "Common.hpp"

#include "String.hpp"

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

U16 ConvertUtf8ToWide(const std::string &sUtf8) {
    return ConvertUtf8ToWide(sUtf8.c_str(), static_cast<int>(sUtf8.size()));
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

U16 ConvertWideToUtf8(const String &sWide) {
    if (sWide.size() != static_cast<U16>(sWide.size()))
        throw ExnArgTooLarge {sWide.size(), 65535};
    return ConvertWideToUtf8(sWide.c_str(), static_cast<int>(sWide.size()));
}

String FormatSize(U64 uSize, PCWSTR pszGiga, PCWSTR pszMega, PCWSTR pszKilo, PCWSTR pszUnit) {
    if (uSize > (1U << 30))
        return Format(L"%.3f %s", static_cast<double>(uSize) / static_cast<double>(1U << 30), pszGiga);
    if (uSize > (1U << 20))
        return Format(L"%.3f %s", static_cast<double>(uSize) / static_cast<double>(1U << 20), pszMega);
    if (uSize > (1U << 10))
        return Format(L"%.3f %s", static_cast<double>(uSize) / static_cast<double>(1U << 10), pszKilo);
    return Format(L"%u %s", static_cast<unsigned>(uSize), pszUnit);
}
