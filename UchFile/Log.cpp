#include "Common.hpp"

#include "Log.hpp"

static void WriteString(HANDLE hFile, const String &sVal) {
    auto uLen = ConvertWideToUtf8(sVal);
    WriteFile(hFile, g_szUtf8Buf, uLen, nullptr, nullptr);
}

template<class ...tvArgs>
static void FilePrintf(HANDLE hFile, PCWSTR pszFmt, tvArgs &&...vArgs) {
    auto uLen = Format(pszFmt, std::forward<tvArgs>(vArgs)...);
    uLen = ConvertWideToUtf8(static_cast<int>(uLen));
    WriteFile(hFile, g_szUtf8Buf, uLen, nullptr, nullptr);
}

static U64 f_usEpoch;

static String FormatStamp(U64 usWhen) {
    auto utElapsed = usWhen - f_usEpoch;
    auto ucHour = std::exchange(utElapsed, utElapsed % 3600'000'000) / 3600'000'000;
    auto ucMinute = std::exchange(utElapsed, utElapsed % 60'000'000) / 60'000'000;
    auto ucSecond = std::exchange(utElapsed, utElapsed % 1'000'000) / 1'000'000;
    auto ucMilli = std::exchange(utElapsed, utElapsed % 1'000) / 1'000;
    return FormatString(
        L"%02" CONCAT(L, PRIu64) L":%02" CONCAT(L, PRIu64) L":%02" CONCAT(L, PRIu64) L".%03" CONCAT(L, PRIu64),
        ucHour, ucMinute, ucSecond, ucMilli
    );
}

void Log::Save(const String &sFileName) {
    f_usEpoch = x_usStart;
    auto uhFile = CreateFileHandle(sFileName, GENERIC_WRITE, CREATE_ALWAYS, 0);
    FilePrintf(
        uhFile.get(),
        L"[%s] "
        L"[file-path %s] "
        L"[file-size %" CONCAT(L, PRIu64) L"] "
        L"Transmition started.\r\n",
        FormatStamp(x_usStart).c_str(), x_sFilePath.c_str(), x_uzFileSize
    );
    for (auto &vEntry : x_vecEntries) {
        FilePrintf(
            uhFile.get(),
            L"[%s] "
            L"[recv-sec %" CONCAT(L, PRIu32) L"] "
            L"[send-sec %" CONCAT(L, PRIu32) L"] "
            L"[file-done %" CONCAT(L, PRIu64) L"] "
            L"Transmitting...\r\n",
            FormatStamp(vEntry.usWhen).c_str(), vEntry.uzRcvdSec, vEntry.uzSentSec, vEntry.uzFile
        );
    }
    auto utElapsed = x_usComplete - x_usStart;
    auto uzAvgRecv = x_uzRcvd * 1'000'000 / utElapsed;
    auto uzAvgSend = x_uzSent * 1'000'000 / utElapsed;
    auto uzAvgFile = x_uzFileSize * 1'000'000 / utElapsed;
    FilePrintf(
        uhFile.get(),
        L"[%s] "
        L"[recv-sec-avg %" CONCAT(L, PRIu64) L"] "
        L"[send-sec-avg %" CONCAT(L, PRIu64) L"] "
        L"[file-sec-avg %" CONCAT(L, PRIu64) L"] "
        L"Transmition completed.\r\n",
        FormatStamp(x_usComplete).c_str(), uzAvgRecv, uzAvgSend, uzAvgFile
    );
}
