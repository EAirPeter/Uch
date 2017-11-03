#include "Common.hpp"

#include "FrmFileSend.hpp"
#include "UccPipl.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmFileSend::FrmFileSend(const String &sWhom, const String &sPath) :
    form(nullptr, {480, 180}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(String {L"Uch - Sending file to ["} + sWhom + L"]...");
    events().destroy(std::bind(&FrmFileSend::X_OnDestroy, this, std::placeholders::_1));
    events().unload([this] (const arg_unload &e) {
        if (x_atmbExitNeedConfirm.load()) {
            msgbox mbx {e.window_handle, u8"Uch - Send file", msgbox::yes_no};
            mbx.icon(msgbox::icon_question);
            mbx << L"Are you sure to cancel?";
            if (mbx() != mbx.pick_yes)
                e.cancel = true;
        }
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmFileSend::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lblName.caption(sPath);
    x_lblState.caption(L"Waiting to be accepted...");
    x_lblProg.text_align(align::right, align_v::center);
    x_pgbProg.amount(x_kucBarAmount);
    x_pl.div(
        "margin=[14,16] vert"
        "   <vfit=448 Name> <weight=7>"
        "   <weight=25 Plbl> <weight=7>"
        "   <weight=25 Pbar> <weight=7>"
        "   <vfit=448 Stat> <>"
        "   <weight=25 <> <weight=81 Canc>>"
    );
    x_pl["Name"] << x_lblName;
    x_pl["Plbl"] << x_lblProg;
    x_pl["Pbar"] << x_pgbProg;
    x_pl["Stat"] << x_lblState;
    x_pl["Canc"] << x_btnCancel;
    x_pl.collocate();
    try {
        x_pPipl = &(*Ucl::Pmg())[sWhom];
    }
    catch (std::out_of_range) {
        x_pPipl = nullptr;
    }
    if (!x_pPipl) {
        msgbox mbx {*this, u8"Uch - Send file", msgbox::ok};
        mbx.icon(msgbox::icon_error);
        mbx << L"The user is not online";
        mbx();
        return;
    }
    x_vSn = x_pPipl->GetLower().GetRemoteSockName();
    HANDLE hFile;
    try {
        hFile = CreateFileHandle(sPath, GENERIC_READ, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED).release();
    }
    catch (ExnSys e) {
        nana::msgbox mbx {u8"Uch - Send file"};
        mbx.icon(nana::msgbox::icon_error);
        mbx << L"Cannot open file, error: " << e.dwError;
        mbx();
        x_atmbExitNeedConfirm.store(false);
        close();
        return;
    }
    x_upFio = std::make_unique<FileIo<X_Proxy>>(x_vProxy, hFile);
    x_uzFileSize = x_upFio->GetSize();
    x_uzUcpSize = (x_uzFileSize + kuzUcpPayload - 1) * kuzUcpHeader + x_uzFileSize;
    x_sFileSize = String {L" / "} + FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B");
    x_lblProg.caption(String {L"0B"} + x_sFileSize);
    Ucl::Bus().Register(*this);
    x_hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszHost], L"0"), false);
    auto *pAddr = reinterpret_cast<const sockaddr_in *>(&GetLocalSockName(x_hSocket));
    x_pPipl->PostPacket(protocol::EvpFileReq {
        reinterpret_cast<U64>(this),
        sPath.substr(sPath.rfind(L'\\') + 1),
        x_uzFileSize, pAddr->sin_port
    });
}

void FrmFileSend::OnEvent(protocol::EvpFileRes &e) noexcept {
    if (e.uId != reinterpret_cast<U64>(this))
        return;
    Ucl::Iog().PostJob([this, e] {
        if (!e.bAccepted) {
            closesocket(x_hSocket);
            x_atmbExitNeedConfirm.store(false);
            close();
            return;
        }
        // ignore the situation that accepted after the sender clicked cancel
        auto *pAddr = reinterpret_cast<sockaddr_in *>(&x_vSn.vSockAddr);
        pAddr->sin_port = e.uPort;
        ::connect(x_hSocket, &x_vSn.vSockAddr, x_vSn.nSockLen);
        x_upUcp = std::make_unique<Ucp<FrmFileSend>>(*this, x_hSocket);
        Ucl::Iog().RegisterTick(*this);
        x_upUcp->AssignToIoGroup(Ucl::Iog());
        x_upFio->AssignToIoGroup(Ucl::Iog());
        for (U32 i = 0; i < x_kucFileChunks; ++i)
            X_PostFileRead();
    });
}

void FrmFileSend::X_OnDestroy(const arg_destroy &e) {
    if (x_atmbNormalExit.load()) {
        while (!(x_bUcpDone && x_bFileDone && x_bTickDone))
            x_cv.Wait(x_mtx);
        return;
    }
    {
        RAII_LOCK(x_mtx);
        x_bTickDone = true;
        if (x_upFio)
            x_upFio->Shutdown();
        else
            x_bFileDone = true;
        if (x_upUcp) {
            try {
                x_upUcp->PostPacket(protocol::EvuFin {});
            }
            catch (...) {
                // suppressed
            }
            x_upUcp->Close();
        }
        else {
            x_bUcpDone = true;
            x_bTickDone = true;
            x_pPipl->PostPacket(protocol::EvpFileCancel {reinterpret_cast<U64>(this)});
        }
        while (!(x_bUcpDone && x_bFileDone && x_bTickDone))
            x_cv.Wait(x_mtx);
        Ucl::Bus().Unregister(*this);
    }
    msgbox mbx {u8"Uch - Send file"};
    mbx.icon(msgbox::icon_information);
    mbx << L"Transmition canceled";
    mbx();
}

bool FrmFileSend::OnTick(U64 usNow) noexcept {
    RAII_LOCK(x_mtx);
    if (x_bUcpDone) {
        x_bTickDone = true;
        x_cv.WakeOne();
        return false;
    }
    if (x_upUcp->GetQueueSize() < x_kuzUcpQueThresh) {
        while (x_atmucFileChunks.fetch_sub(1))
            X_PostFileRead();
        x_atmucFileChunks.fetch_add(1);
    }
    if (x_atmbUpdating.test_and_set()) {
        // lag
        return true;
    }
    if (StampDue(usNow, x_usNextUpd)) {
        Ucl::Iog().PostJob([this, usNow] {
            x_usNextUpd = usNow + 200'000;
            auto uzSent = x_upUcp->GetSentSize();
            x_lblProg.caption(
                FormatSize(uzSent, L"GiB", L"MiB", L"KiB", L"B") + x_sFileSize
            );
            x_pgbProg.value(static_cast<U32>(x_kucBarAmount * uzSent / x_uzFileSize));
            if (StampDue(usNow, x_usNextSec)) {
                x_usNextSec = usNow + 1'000'000;
                auto uzSentSec = uzSent - std::exchange(x_uzSentSec, uzSent);
                auto &&sState = String {L"Transmitting @ "} +
                    FormatSize(uzSentSec, L"GiB/s...", L"MiB/s...", L"KiB/s...", L"B/s...");
                x_lblState.caption(std::move(sState));
            }
            x_atmbUpdating.clear();
        });
    }
    else {
        x_atmbUpdating.clear();
    }
    return true;
}

void FrmFileSend::OnPacket(U32 uSize, UcpBuffer &vBuf) noexcept {
    using namespace protocol;
    auto byId = vBuf.Read<Byte>();
    switch (byId) {
    case ucpfile::kFin:
        x_upUcp->PostPacket(EvuFinAck {});
        x_upUcp->EndOnPacket(uSize);
        x_upUcp->Shutdown();
        x_atmbNormalExit.store(true);
        x_atmbExitNeedConfirm.store(false);
        close();
        break;
    case ucpfile::kFinAck:
        x_upUcp->EndOnPacket(uSize);
        x_upUcp->Shutdown();
        x_atmbNormalExit.store(true);
        x_atmbExitNeedConfirm.store(false);
        close();
        break;
    default:
        assert(false);
    }
}

void FrmFileSend::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bUcpDone = true;
    x_cv.WakeOne();
}

void FrmFileSend::OnForciblyClose() noexcept {}

void FrmFileSend::X_PostFileRead() noexcept {
    auto uPos = x_atmuzFilePos.fetch_add(FileChunk::kCapacity);
    if (uPos >= x_uzFileSize) {
        x_upFio->Shutdown();
        return;
    }
    auto upChunk = x_vFcp.MakeUnique();
    upChunk->Offset = static_cast<DWORD>(uPos);
    upChunk->OffsetHigh = static_cast<DWORD>(uPos >> 32);
    x_upFio->PostRead(upChunk.release());
}

void FrmFileSend::X_Proxy::OnFinalize() noexcept {
    RAII_LOCK(pFrm->x_mtx);
    pFrm->x_bFileDone = true;
    pFrm->x_cv.WakeOne();
}

void FrmFileSend::X_Proxy::OnForciblyClose() noexcept {}

void FrmFileSend::X_Proxy::OnRead(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept {
    auto upChunk = pFrm->x_vFcp.Wrap(pChunk);
    auto uOff = static_cast<U64>(upChunk->OffsetHigh) << 32 | upChunk->Offset;
    try {
        pFrm->x_upUcp->PostPacket(protocol::EvuFile {uOff, std::move(upChunk)});
    }
    catch (ExnIllegalState) {
        // shutting down
        return;
    }
    if (pFrm->x_upUcp->GetQueueSize() < x_kuzUcpQueThresh)
        pFrm->X_PostFileRead();
    else
        pFrm->x_atmucFileChunks.fetch_add(1);
}
