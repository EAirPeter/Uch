#include "Common.hpp"

#include "FrmFileRecv.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmFileRecv::FrmFileRecv(const event::EvFileRecv &e) :
    form(nullptr, {480, 180}, appear::decorate<appear::taskbar, appear::minimize>()),
    x_uzFileSize(e.uSize), x_uzUcpSize((e.uSize + kuzUcpPayload - 1) * kuzUcpHeader + e.uSize),
    x_sFileSize(String {L" / "} + FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B")),
    x_sFilePath(e.sPath)
{
    caption(String {L"Uch - Receiving file from ["} + e.pPipl->GetUser() + L"]");
    events().destroy(std::bind(&FrmFileRecv::X_OnDestroy, this, std::placeholders::_1));
    events().unload([this] (const arg_unload &e) {
        if (x_atmbExitNeedConfirm.load()) {
            msgbox mbx {e.window_handle, u8"Uch - Receive file", msgbox::yes_no};
            mbx.icon(msgbox::icon_question);
            mbx << L"Are you sure to cancel?";
            if (mbx() != mbx.pick_yes)
                e.cancel = true;
        }
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmFileRecv::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lblName.caption(e.sPath);
    x_lblState.caption(L"Transmitting @ 0 B/s...");
    x_lblProg.text_align(align::right, align_v::center).caption(String {L"0B"} + x_sFileSize);
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
    HANDLE hFile;
    try {
        hFile = CreateFileHandle(
            x_sFilePath, GENERIC_WRITE, CREATE_ALWAYS,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
        );
        LARGE_INTEGER vLi;
        vLi.QuadPart = static_cast<I64>(
            (x_uzFileSize + FileChunk::kCapacity - 1) & ~static_cast<U64>(FileChunk::kCapacity - 1)
        );
        auto dwb = SetFilePointerEx(hFile, vLi, nullptr, FILE_BEGIN);
        if (!dwb)
            throw ExnSys();
        dwb = SetEndOfFile(hFile);
        if (!dwb)
            throw ExnSys();
    }
    catch (ExnSys e_) {
        e.pPipl->PostPacket(protocol::EvpFileRes {
            e.uId, false, 0
        });
        nana::msgbox mbx {u8"Uch - Send file"};
        mbx.icon(nana::msgbox::icon_error);
        mbx << L"Cannot open file, error: " << e_.dwError;
        mbx();
        x_atmbExitNeedConfirm.store(false);
        close();
        return;
    }
    x_upFio = std::make_unique<FileIo<X_Proxy>>(x_vProxy, hFile);
    auto hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszHost], L"0"), false);
    auto vSn = e.pPipl->GetLower().GetRemoteSockName();
    auto *pAddr = reinterpret_cast<sockaddr_in *>(&vSn);
    pAddr->sin_port = e.uPort;
    ::connect(hSocket, &vSn.vSockAddr, vSn.nSockLen);
    auto *pAddr_ = reinterpret_cast<const sockaddr_in *>(&GetLocalSockName(hSocket));
    x_upUcp = std::make_unique<Ucp<FrmFileRecv>>(*this, hSocket);
    Ucl::Iog().RegisterTick(*this);
    x_upFio->AssignToIoGroup(Ucl::Iog());
    x_upUcp->AssignToIoGroup(Ucl::Iog());
    pAddr_ = reinterpret_cast<const sockaddr_in *>(&x_upUcp->GetLower().GetLocalSockName());
    e.pPipl->PostPacket(protocol::EvpFileRes {
        e.uId, true, pAddr_->sin_port
    });
}

void FrmFileRecv::X_OnDestroy(const arg_destroy &e) {
    if (x_atmbNormalExit.load()) {
        while (!(x_bUcpDone && x_bFileDone && x_bTickDone && x_bEofDone))
            x_cv.Wait(x_mtx);
        return;
    }
    x_upFio->Shutdown();
    try {
        x_upUcp->PostPacket(protocol::EvuFin {});
    }
    catch (...) {
        // suppressed
    }
    x_upUcp->Shutdown();
    {
        RAII_LOCK(x_mtx);
        x_bEofDone = true;
        while (!(x_bUcpDone && x_bFileDone && x_bTickDone && x_bEofDone))
            x_cv.Wait(x_mtx);
    }
    msgbox mbx {u8"Uch - Receive file"};
    mbx.icon(msgbox::icon_information);
    mbx << L"Transmition canceled";
    mbx();
}

bool FrmFileRecv::OnTick(U64 usNow) noexcept {
    RAII_LOCK(x_mtx);
    if (x_bUcpDone) {
        x_bTickDone = true;
        x_cv.WakeAll();
        return false;
    }
    if (x_atmbUpdating.test_and_set()) {
        // lag
        return true;
    }
    if (StampDue(usNow, x_usNextUpd)) {
        Ucl::Iog().PostJob([this, usNow] {
            x_usNextUpd = usNow + 200'000;
            auto uzRcvd = x_upUcp->GetReceivedSize();
            x_lblProg.caption(
                FormatSize(uzRcvd, L"GiB", L"MiB", L"KiB", L"B") + x_sFileSize
            );
            x_pgbProg.value(static_cast<U32>(x_kucBarAmount * uzRcvd / x_uzFileSize));
            if (StampDue(usNow, x_usNextSec)) {
                x_usNextSec = usNow + 1'000'000;
                auto uzRcvdSec = uzRcvd - std::exchange(x_uzRcvdSec, uzRcvd);
                auto &&sState = String {L"Transmitting @ "} +
                    FormatSize(uzRcvdSec, L"GiB/s...", L"MiB/s...", L"KiB/s...", L"B/s...");
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

void FrmFileRecv::OnPacket(U32 uSize, UcpBuffer &vBuf) noexcept {
    using namespace protocol;
    auto byId = vBuf.Read<Byte>();
    switch (byId) {
    case ucpfile::kFile:
        {
            EvuFile e;
            e.upChunk = x_vFcp.MakeUnique();
            vBuf >> e;
            e.upChunk->IncWriter(e.upChunk->GetWritable());
            e.upChunk->Offset = static_cast<DWORD>(e.uOffset);
            e.upChunk->OffsetHigh = static_cast<DWORD>(e.uOffset >> 32);
            e.upChunk->pParam = reinterpret_cast<void *>(static_cast<UPtr>(uSize));
            x_upFio->Write(e.upChunk.release());
        }
        break;
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

void FrmFileRecv::OnFinalize() noexcept {
    RAII_LOCK(x_mtx);
    x_bUcpDone = true;
    x_cv.WakeAll();
}

void FrmFileRecv::OnForciblyClose() noexcept {}

void FrmFileRecv::X_Proxy::OnFinalize() noexcept {
    RAII_LOCK(pFrm->x_mtx);
    pFrm->x_bFileDone = true;
    pFrm->x_cv.WakeAll();
}

void FrmFileRecv::X_Proxy::OnForciblyClose() noexcept {}

void FrmFileRecv::X_Proxy::OnWrite(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept {
    {
        auto upChunk = pFrm->x_vFcp.Wrap(pChunk);
        pFrm->x_upUcp->EndOnPacket(static_cast<U32>(reinterpret_cast<UPtr>(upChunk->pParam)));
    }
    auto uPos = pFrm->x_atmuzFilePos.fetch_add(FileChunk::kCapacity);
    if (uPos + FileChunk::kCapacity < pFrm->x_uzFileSize)
        return;
    Ucl::Iog().PostJob([this] {
        RAII_LOCK(pFrm->x_mtx);
        while (!pFrm->x_bFileDone)
            pFrm->x_cv.Wait(pFrm->x_mtx);
        std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> hFile (
            CreateFileHandle(pFrm->x_sFilePath, GENERIC_WRITE, OPEN_EXISTING, 0),
            &CloseHandle
        );
        LARGE_INTEGER vLi;
        vLi.QuadPart = static_cast<I64>(pFrm->x_uzFileSize);
        auto dwb = SetFilePointerEx(hFile.get(), vLi, nullptr, FILE_BEGIN);
        if (!dwb)
            throw ExnSys();
        dwb = SetEndOfFile(hFile.get());
        if (!dwb)
            throw ExnSys();
        pFrm->x_bEofDone = true;
        pFrm->x_cv.WakeAll();
    });
    pFrm->x_upUcp->PostPacket(protocol::EvuFin {});
    pFrm->x_upFio->Shutdown();
}
