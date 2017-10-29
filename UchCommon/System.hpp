#pragma once

#include "Common.hpp"

class System {
public:
    static void GlobalStartup();

private:
    System();
    ~System();

};

USize GetProcessors() noexcept;

HANDLE CreateFileHandle(const String &sPath, DWORD dwAccess, DWORD dwCreation, DWORD dwFlags);

U64 GetTimeStamp() noexcept;

constexpr U64 StampInfinite(U64 usNow) noexcept {
    return usNow + 0x8000000000000000ULL;
}

constexpr bool StampBefore(U64 usSub, U64 usObj) noexcept {
    return static_cast<I64>(usSub - usObj) < 0;
}

constexpr bool StampDue(U64 usNow, U64 usDue) noexcept {
    return static_cast<I64>(usNow - usDue) >= 0;
}
