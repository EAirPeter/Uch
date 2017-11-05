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

HANDLE HandleCast(UPtr uHandle);

int RecvMain(PCWSTR pszCmdLine);
int SendMain(PCWSTR pszCmdLine);
