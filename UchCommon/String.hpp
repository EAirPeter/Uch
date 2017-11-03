#pragma once

#include "Common.hpp"

constexpr static USize STRCVT_BUFSIZE = 65536;

extern thread_local char g_szUtf8Buf[STRCVT_BUFSIZE];
extern thread_local wchar_t g_szWideBuf[STRCVT_BUFSIZE];

U16 ConvertUtf8ToWide(const char *pszUtf8, int nLen = -1);
U16 ConvertUtf8ToWide(const std::string &sUtf8);
U16 ConvertWideToUtf8(const wchar_t *pszWide, int nLen = -1);
U16 ConvertWideToUtf8(const String &sWide);

inline U16 ConvertUtf8ToWide(int nLen = -1) {
    return ConvertUtf8ToWide(g_szUtf8Buf, nLen);
}

inline U16 ConvertWideToUtf8(int nLen = -1) {
    return ConvertWideToUtf8(g_szWideBuf, nLen);
}

inline std::string AsUtf8String(const String &sWide) {
    return {g_szUtf8Buf, ConvertWideToUtf8(sWide)};
}

inline String AsWideString(const std::string &sUtf8) {
    return {g_szWideBuf, ConvertUtf8ToWide(sUtf8)};
}

template<class ...tvArgs>
inline U16 Format(const wchar_t *pszFmt, tvArgs &&...vArgs) {
    auto nRes = swprintf_s(g_szWideBuf, STRCVT_BUFSIZE, pszFmt, std::forward<tvArgs>(vArgs)...);
    if (nRes == -1)
        throw ExnIllegalArg {};
    return static_cast<U16>(nRes);
}

template<class ...tvArgs>
inline String FormatString(const wchar_t *pszFmt, tvArgs &&...vArgs) {
    auto uLen = Format(pszFmt, std::forward<tvArgs>(vArgs)...);
    return {g_szWideBuf, uLen};
}

String FormatSize(U64 uSize, PCWSTR pszGiga, PCWSTR pszMega, PCWSTR pszKilo, PCWSTR pszUnit);
