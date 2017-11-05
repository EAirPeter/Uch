#pragma once

#include "Common.hpp"

class Log {
private:
    struct X_Entry {
        U64 usWhen;
        U32 uzRcvdSec;
        U32 uzSentSec;
        U64 uzFile;
    };

public:
    inline Log(String sFilePath, U64 uzFileSize) noexcept :
        x_sFilePath(sFilePath), x_uzFileSize(uzFileSize) {}

    Log(const Log &) = delete;
    Log(Log &&) = delete;

    Log &operator =(const Log &) = delete;
    Log &operator =(Log &&) = delete;

    inline void TimeStart() {
        x_usStart = GetTimeStamp();
    }

    inline void Record(U64 usNow, U32 uzRcvdSec, U32 uzSentSec, U64 uzFile) {
        x_vecEntries.emplace_back(X_Entry {usNow, uzRcvdSec, uzSentSec, uzFile});
    }

    inline void TimeStop() {
        x_usComplete = GetTimeStamp();
    }

    inline void SetTotal(U64 uzRcvdTotal, U64 uzSentTotal) {
        x_uzRcvd = uzRcvdTotal;
        x_uzSent = uzSentTotal;
    }

    void Save(const String &sFileName);

private:
    U64 x_usStart;
    U64 x_usComplete;
    U64 x_uzFileSize;
    U64 x_uzRcvd;
    U64 x_uzSent;
    String x_sFilePath;
    std::vector<X_Entry> x_vecEntries;

};
