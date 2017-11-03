#pragma once

#include "Common.hpp"

#include "../UchCommon/FileIo.hpp"
#include "../UchCommon/Ucp.hpp"

#include "Event.hpp"
#include "UccPipl.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/progress.hpp>

class FrmFileRecv : public nana::form {
public:
    FrmFileRecv(const event::EvFileRecv &e);

private:
    void X_OnDestroy(const nana::arg_destroy &e);

public:
    bool OnTick(U64 usNow) noexcept;
    void OnPacket(U32 uSize, UcpBuffer &vBuf) noexcept;
    void OnFinalize() noexcept;
    void OnForciblyClose() noexcept;

private:
    nana::place x_pl {*this};
    nana::button x_btnCancel {*this};
    nana::label x_lblName {*this};
    nana::label x_lblState {*this};
    nana::label x_lblProg {*this};
    nana::progress x_pgbProg {*this};

private:
    struct X_Proxy {
        void OnFinalize() noexcept;
        void OnForciblyClose() noexcept;
        void OnWrite(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept;

        FrmFileRecv *pFrm;
    };

private:
    FileChunkPool x_vFcp;

    RecursiveMutex x_mtx;
    ConditionVariable x_cv;
    bool x_bUcpDone = false;
    bool x_bFileDone = false;
    bool x_bTickDone = false;
    bool x_bEofDone = false;
    std::atomic<bool> x_atmbNormalExit = false;
    std::atomic<bool> x_atmbExitNeedConfirm = true;

    X_Proxy x_vProxy {this};
    UccPipl *x_pPipl = nullptr;
    std::unique_ptr<Ucp<FrmFileRecv>> x_upUcp;
    std::unique_ptr<FileIo<X_Proxy>> x_upFio;
    U64 x_uzFileSize = 0;
    U64 x_uzUcpSize = 0;
    String x_sFileSize;
    String x_sFilePath;
    std::atomic<U64> x_atmuzFilePos = 0;
    std::atomic<U32> x_atmucFileChunks = 0;
    U64 x_uzRcvdSec = 0;
    U64 x_usNextSec = 0;
    U64 x_usNextUpd = 0;
    std::atomic_flag x_atmbUpdating {};
    
private:
    constexpr static U32 x_kucBarAmount = 1U << 12;

};
