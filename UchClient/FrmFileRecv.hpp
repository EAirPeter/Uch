#pragma once

#include "Common.hpp"

#include "UccPipl.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/progress.hpp>

class FrmFileRecv :
    public nana::form,
    public HandlerBase<
        protocol::EvpFileCancel
    >
{
public:
    FrmFileRecv(const nana::form &frmParent, UccPipl *pPipl, const protocol::EvpFileReq &e);

public:
    void OnEvent(protocol::EvpFileCancel &e) noexcept override;

private:
    void X_OnDestroy(const nana::arg_destroy &e);
    void X_OnUser();

public:
    bool OnTick(U64 usNow) noexcept;

private:
    nana::place x_pl {*this};
    nana::button x_btnCancel {*this};
    nana::label x_lblName {*this};
    nana::label x_lblState {*this};
    nana::label x_lblProg {*this};
    nana::progress x_pgbProg {*this};

private:
    U64 x_uId;
    UccPipl *x_pPipl;
    String x_sFileName;
    String x_sFilePath;
    U64 x_uzFileSize;
    std::string x_su8MbxTitle;
    PROCESS_INFORMATION x_vPi {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, 0, 0};
    HANDLE x_hFile = INVALID_HANDLE_VALUE;
    SOCKET x_hSocket = INVALID_SOCKET;
    HANDLE x_hEvent = INVALID_HANDLE_VALUE;
    HANDLE x_hMapping = INVALID_HANDLE_VALUE;
    HANDLE x_hMutex = INVALID_HANDLE_VALUE;
    
    bool x_bAskCancel = true;
    bool x_bCanceling = true;
    
    std::atomic_flag x_atmbUpd {};

    Mutex x_mtx;

private:
    constexpr static U32 x_kucBarAmount = 4096;

};
