#include "Common.hpp"

int wmain(int nArgs, wchar_t *apszArgs[]) {
    System::GlobalStartup();
    if (nArgs < 2)
        return EXIT_FAILURE;
    if (!lstrcmpW(apszArgs[1], L"+"))
        return RecvMain(nArgs, apszArgs);
    if (!lstrcmpW(apszArgs[1], L"-"))
        return SendMain(nArgs, apszArgs);
    return EXIT_FAILURE;
}
