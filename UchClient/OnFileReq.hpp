#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

class OnFileReq :
    HandlerBase<
        protocol::EvpFileCancel
    >
{
    friend class UccPipl;

private:
    OnFileReq(UccPipl *pPipl, const protocol::EvpFileReq &e);
    ~OnFileReq();

public:
    void OnEvent(protocol::EvpFileCancel &e) noexcept override;

private:
    void X_Run();

private:
    UccPipl *x_pPipl;
    protocol::EvpFileReq x_eReq;
    std::atomic<bool> x_atmbCanceled = false;

};
