#pragma once

#include "Common.hpp"

#include "../UchCommon/FileIo.hpp"
#include "../UchCommon/Ucp.hpp"

#include "UccPipl.hpp"

#include <nana/gui.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/progress.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmFileSend :
    public nana::form,
    public HandlerBase<
        protocol::EvpFileRes
    >
{
public:
    FrmFileSend(const String &sWhom, const String &sPath);

public:
    void OnEvent(protocol::EvpFileRes &e) noexcept override;

private:
    void X_OnDestroy(const nana::arg_destroy &e);

public:
    bool OnTick(U64 usNow) noexcept;
    void OnPacket(U32 uSize, UcpBuffer &vBuf) noexcept;
    void OnFinalize() noexcept;
    void OnForciblyClose() noexcept;

private:
    void X_PostFileRead() noexcept;

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
        void OnRead(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept;

        FrmFileSend *pFrm;
    };

private:
    FileChunkPool x_vFcp;

    RecursiveMutex x_mtx;
    ConditionVariable x_cv;
    bool x_bUcpDone = false;
    bool x_bFileDone = false;
    bool x_bTickDone = false;
    std::atomic<bool> x_atmbNormalExit = false;
    std::atomic<bool> x_atmbExitNeedConfirm = true;

    X_Proxy x_vProxy {this};
    UccPipl *x_pPipl = nullptr;
    SOCKET x_hSocket = INVALID_SOCKET;
    std::unique_ptr<Ucp<FrmFileSend>> x_upUcp;
    std::unique_ptr<FileIo<X_Proxy>> x_upFio;
    SockName x_vSn {};
    U64 x_uzFileSize = 0;
    U64 x_uzUcpSize = 0;
    String x_sFileSize;
    std::atomic<U64> x_atmuzFilePos = 0;
    std::atomic<U32> x_atmucFileChunks = 0;
    U64 x_uzSentSec = 0;
    U64 x_usNextSec = 0;
    U64 x_usNextUpd = 0;
    std::atomic_flag x_atmbUpdating {};

private:
    constexpr static U32 x_kucBarAmount = 1U << 12;
    constexpr static U32 x_kucFileChunks = 4;
    constexpr static U32 x_kuzUcpQueThresh = FileChunk::kCapacity * x_kucFileChunks;

};
