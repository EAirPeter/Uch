#pragma once
#include "../UchCommon/Common.hpp"

#include "../UchCommon/FileIo.hpp"
#include "../UchCommon/IoGroup.hpp"
#include "../UchCommon/String.hpp"
#include "../UchCommon/Sync.hpp"
#include "../UchCommon/System.hpp"
#include "../UchCommon/Ucp.hpp"
#include "../UchCommon/Wsa.hpp"

#include "../UchProtocol/Event.hpp"

void PrintConsole(const String &s);

template<class ...tvArgs>
inline void Printf(PCWSTR pszFmt, tvArgs &&...vArgs) {
    PrintConsole(FormatString(pszFmt, std::forward<tvArgs>(vArgs)...));
}

int RecvMain(int nArgs, wchar_t *apszArgs[]);
int SendMain(int nArgs, wchar_t *apszArgs[]);
