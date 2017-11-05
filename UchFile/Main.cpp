#include "Common.hpp"

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        auto pszCmdLine = GetCommandLineW();
        System::GlobalStartup();
        if (swscanf_s(pszCmdLine, L"%*s%s", g_szWideBuf, static_cast<unsigned>(STRCVT_BUFSIZE)) != 1)
            throw ExnIllegalArg {};
        if (!lstrcmpW(g_szWideBuf, L"+"))
            return RecvMain(pszCmdLine);
        if (!lstrcmpW(g_szWideBuf, L"-"))
            return SendMain(pszCmdLine);
        throw ExnIllegalArg {};
    }
    catch (ExnIllegalArg) {
        MessageBoxW(
            nullptr,
            L"Unexpected argument detected",
            L"Uch - File transmitting",
            MB_OK | MB_ICONERROR
        );
        ExitProcess(ERROR_INVALID_PARAMETER);
    }
    catch (ExnIllegalState) {
        MessageBoxW(
            nullptr,
            L"Unexpected state detected",
            L"Uch - File transmitting",
            MB_OK | MB_ICONERROR
        );
        ExitProcess(ERROR_INVALID_STATE);
    }
    catch (ExnSys e) {
        MessageBoxW(
            nullptr,
            FormatString(
                L"Unexpected system error detected: %u",
                static_cast<unsigned>(e.dwError)
            ).c_str(),
            L"Uch - File transmitting", MB_OK | MB_ICONERROR
        );
        ExitProcess(static_cast<UINT>(e.dwError));
    }
    catch (ExnWsa e) {
        MessageBoxW(
            nullptr,
            FormatString(
                L"Unexpected winsock error detected: %u",
                static_cast<unsigned>(e.nError)
            ).c_str(),
            L"Uch - File transmitting", MB_OK | MB_ICONERROR
        );
        ExitProcess(static_cast<UINT>(e.nError));
    }
    return EXIT_FAILURE;
}
