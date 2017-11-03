#pragma once

#include "Common.hpp"

class System {
public:
    static void GlobalStartup();

private:
    System();
    ~System();

};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)>;

U32 GetProcessors() noexcept;

UniqueHandle CreateFileHandle(const String &sPath, DWORD dwAccess, DWORD dwCreation, DWORD dwFlags);
HANDLE CreateFileHandleInherit(const String &sPath, DWORD dwAccess, DWORD dwCreation, DWORD dwFlags);

U64 GetFileSize(HANDLE hFile);
void SetFileSize(HANDLE hFile, U64 uSize);

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

String FormattedTime() noexcept;
