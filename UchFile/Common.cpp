#include "Common.hpp"

HANDLE HandleCast(UPtr uHandle) {
    auto hVal = reinterpret_cast<HANDLE>(uHandle);
    DWORD dw;
    if (!GetHandleInformation(reinterpret_cast<HANDLE>(hVal), &dw))
        throw ExnSys();
    return hVal;
}
