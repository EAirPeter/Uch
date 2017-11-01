#include "Common.hpp"

#include "FrmFileSend.hpp"
#include "UccPipl.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmFileSend::FrmFileSend(const form &frmParent, const String &sWhom, const String &sPath) :
    form(frmParent, {480, 180}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(String {L"Uch-Sending file to ["} + sWhom + L"]...");
    events().unload(std::bind(&FrmFileSend::X_OnDestroy, this, std::placeholders::_1));
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmFileSend::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lblName.caption(sPath);
    x_lblState.caption(L"Waiting to be accepted...");
    x_lblProg.text_align(align::right, align_v::center).caption(L"0.0%");
    x_pgbProg.amount(x_kucBarAmount);
    x_pl.div(
        "margin=[14,16] vert"
        "   <vfit=448 Name> <weight=7>"
        "   <weight=25 arrange=[40,variable] gap=8 Prog> <weight=7>"
        "   <vfit=448 Stat> <>"
        "   <weight=25 <> <weight=81 Canc>>"
    );
    x_pl["Name"] << x_lblName;
    x_pl["Prog"] << x_lblProg << x_pgbProg;
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
        hFile = CreateFileHandle(sPath, GENERIC_READ, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED);
    }
    catch (ExnSys e) {
        nana::msgbox mbx {u8"Uch - Send file"};
        mbx.icon(nana::msgbox::icon_error);
        mbx << L"Cannot open file, error: " << e.dwError;
        mbx();
        return;
    }
    x_upFio = std::make_unique<FileIo<X_Proxy>>(x_vProxy, hFile);
    x_uzFileSize = x_upFio->GetSize();
    x_uzUcpSize = (x_uzFileSize + kuzUcpPayload - 1) * kuzUcpHeader + x_uzFileSize;
    Ucl::Bus().Register(*this);
    x_pPipl->PostPacket(protocol::EvpFileReq {
        reinterpret_cast<U64>(this),
        sPath.substr(sPath.rfind(L'\\') + 1),
        x_uzFileSize
    });
}

void FrmFileSend::OnEvent(protocol::EvpFileRes &e) noexcept {
    if (e.uId != reinterpret_cast<U64>(this))
        return;
    Ucl::Iog().PostJob([this, e] {
        // ignore the situation that accepted after the sender clicked cancel
        auto *pAddr = reinterpret_cast<sockaddr_in *>(&x_vSn.vSockAddr);
        pAddr->sin_port = e.uPort;
        x_upUcp = std::make_unique<Ucp<FrmFileSend>>(*this, CreateConnectedSocket(x_vSn, false));
        x_upUcp->AssignToIoGroup(Ucl::Iog());
        x_upFio->AssignToIoGroup(Ucl::Iog());
        Ucl::Iog().RegisterTick(*this);
        for (U32 i = 0; i < x_kucFileChunks; ++i)
            X_PostFileRead();
    });
}

void FrmFileSend::X_OnDestroy(const arg_unload &e) {
    if (x_atmbExitNeedConfirm.load()) {
        msgbox mbx {e.window_handle, u8"Uch - Send file", msgbox::yes_no};
        mbx.icon(msgbox::icon_question);
        mbx << L"Are you sure to cancel?";
        if (mbx() != mbx.pick_yes) {
            e.cancel = true;
            return;
        }
    }
    RAII_LOCK(x_mtx);
    if (!(x_bUcpDone && x_bFileDone && x_bTickDone)) {
        x_bStopping = true;
        x_upFio->Shutdown();
        if (x_upUcp) {
            try {
                x_upUcp->PostPacket(protocol::EvuFin {});
            }
            catch (...) {
                // suppressed
            }
            x_upUcp->Shutdown();
        }
        else {
            x_bUcpDone = true;
            x_bTickDone = true;
            x_pPipl->PostPacket(protocol::EvpFileCancel {reinterpret_cast<U64>(this)});
        }
        while (!(x_bUcpDone && x_bFileDone && x_bTickDone))
            x_cv.Wait(x_mtx);
    }
    Ucl::Bus().Unregister(*this);
    if (x_atmuzFilePos.load() < x_uzFileSize || (x_upUcp && x_upUcp->GetQueueSize())) {
        msgbox mbx {e.window_handle, u8"Uch - Send file", msgbox::ok};
        mbx.icon(msgbox::icon_information);
        mbx << L"Transmition canceled";
        mbx();
    }
}

bool FrmFileSend::OnTick(U64 usNow) noexcept {
    RAII_LOCK(x_mtx);
    if (x_bStopping) {
        x_bTickDone = true;
        x_cv.WakeOne();
        return false;
    }
    if (x_upUcp->GetQueueSize() >= x_kuzUcpQueThresh)
        return true;
    while (x_atmucFileChunks.fetch_sub(1))
        X_PostFileRead();
    x_atmucFileChunks.fetch_add(1);
    if (x_atmbUpdating.test_and_set()) {
        // lag
        return true;
    }
    if (StampDue(usNow, x_usNextUpd)) {
        Ucl::Iog().PostJob([this, usNow] {
            x_usNextUpd = usNow + 200'000;
            auto uzSent = x_upUcp->GetSentSize();
            x_lblProg.caption(
                Format(L"%.1f%%", static_cast<double>(uzSent) / static_cast<double>(x_uzFileSize))
            );
            x_pgbProg.value(static_cast<U32>(x_kucBarAmount * uzSent / x_uzFileSize));
            if (StampDue(usNow, x_usNextSec)) {
                x_usNextSec = usNow + 1'000'000;
                auto uzSentSec = uzSent - std::exchange(x_uzSentSec, uzSent);
                auto &&sState = uzSentSec > (1U << 20) ?
                    Format(
                        L"Transmitting @ %.3f MiB/s...",
                        static_cast<double>(uzSentSec) / static_cast<double>(1U << 20)
                    ) : uzSentSec > (1U << 10) ?
                    Format(
                        L"Transmitting @ %.3f KiB/s...",
                        static_cast<double>(uzSentSec) / static_cast<double>(1U << 10)
                    ) :
                    Format(
                        L"Transmitting @ %u B/s...",
                        static_cast<unsigned>(uzSentSec)
                    );
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
        x_atmbExitNeedConfirm.store(false);
        close();
        break;
    case ucpfile::kFinAck:
        x_upUcp->EndOnPacket(uSize);
        Ucl::Iog().PostJob(&Ucp<FrmFileSend>::Close, x_upUcp.get());
        x_atmbExitNeedConfirm.store(false);
        close();
        break;
    default:
        assert(false);
    }
}

void FrmFileSend::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bStopping = true;
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

void FrmFileSend::X_Proxy::OnFinalize() {
    RAII_LOCK(pFrm->x_mtx);
    pFrm->x_bFileDone = true;
    pFrm->x_cv.WakeOne();
}

void FrmFileSend::X_Proxy::OnForciblyClose() {}

void FrmFileSend::X_Proxy::OnRead(DWORD dwError, U32 uDone, FileChunk *pChunk) {
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
