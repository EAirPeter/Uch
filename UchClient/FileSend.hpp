#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

class FileSend : HandlerBase<protocol::EvpFileRes>
{
public:
    FileSend(UccPipl *pPipl, const String &sPath);
    ~FileSend();

public:
    void OnEvent(protocol::EvpFileRes &e) noexcept override;

private:
    HANDLE x_hFile = INVALID_HANDLE_VALUE;
    SOCKET x_hSocket = INVALID_SOCKET;
    UccPipl *x_pPipl = nullptr;
    String x_sName;

};
