#include "Common.hpp"

#include "../UchFile/IpcData.hpp"

#include "FrmFileRecv.hpp"
#include "Ucl.hpp"

#include <nana/gui/filebox.hpp>

using namespace nana;

FrmFileRecv::FrmFileRecv(const nana::form &frmParent, UccPipl *pPipl, const protocol::EvpFileReq &e) :
    form(frmParent, {480, 240}, appear::decorate<appear::taskbar, appear::minimize>()),
    x_uId(e.uId), x_pPipl(pPipl), x_uzFileSize(e.uSize)
{
    RAII_LOCK(x_mtx);
    Ucl::Bus().Register(*this);
    msgbox mbx {frmParent, TitleU8(L"Receive file"), msgbox::yes_no};
    mbx.icon(msgbox::icon_question);
    mbx << e.sName << L" [" << FormatSize(e.uSize, L"GiB", L"MiB", L"KiB", L"B") << L"]\n";
    mbx << L"Receive the file from [" << pPipl->GetUser() << L"]?";
    if (mbx() != msgbox::pick_yes) {
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId, false, 0
        });
        throw ExnIllegalState {};
    }
    filebox fbx {frmParent, false};
    fbx.init_file(AsUtf8String(e.sName));
    if (!fbx()) {
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId, false, 0
        });
        throw ExnIllegalState {};
    }
    x_sFilePath = AsWideString(fbx.file());
    x_sFileName = x_sFilePath.substr(x_sFilePath.rfind(L'\\') + 1);
    x_su8MbxTitle = TitleU8(FormatString(L"Receive [%s] from [%s]", x_sFileName.c_str(), pPipl->GetUser().c_str()));
    caption(Title(FormatString(L"Receiving [%s] from [%s]...", x_sFileName.c_str(), pPipl->GetUser().c_str())));
    events().destroy(std::bind(&FrmFileRecv::X_OnDestroy, this, std::placeholders::_1));
    events().user(std::bind(&FrmFileRecv::X_OnUser, this));
    events().unload([this] (const arg_unload &e) {
        if (x_bAskCancel) {
            msgbox mbx {*this, x_su8MbxTitle, msgbox::yes_no};
            mbx.icon(msgbox::icon_question);
            mbx << L"Are you sure to cancel?";
            if (mbx() != mbx.pick_yes) {
                e.cancel = true;
                return;
            }
            x_pPipl->PostPacket(protocol::EvpFileCancel {x_uId});
            x_bCanceling = true;
        }
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmFileRecv::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lblName.caption(x_sFilePath);
    x_lblState.caption(L"Receiving...\nUpload = 0 B/s\nDownload = 0 B/s");
    x_lblProg.text_align(align::right, align_v::center).caption(String {L"0B"});
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
    x_hFile = CreateFileHandleInherit(
        x_sFilePath, GENERIC_WRITE, CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
    );
    x_hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszLisHost], L"0"), false);
    auto vSn = pPipl->GetLower().GetRemoteSockName();
    vSn.SetPort(e.uPort);
    ::connect(x_hSocket, &vSn.vSockAddr, vSn.nSockLen);
    vSn = GetLocalSockName(x_hSocket);
    SECURITY_ATTRIBUTES vSecAttr {static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)), nullptr, true};
    x_hEvent = CreateEventW(&vSecAttr, TRUE, FALSE, nullptr);
    x_hMapping = CreateFileMappingW(
        nullptr, &vSecAttr, PAGE_READWRITE,
        0, static_cast<DWORD>(sizeof(uchfile::IpcData)), nullptr
    );
    x_hMutex = CreateMutexW(&vSecAttr, FALSE, nullptr);
    USize uAttrListSize;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &uAttrListSize);
    STARTUPINFOEXW vSix {
        {static_cast<DWORD>(sizeof(STARTUPINFOEXW))},
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(::operator new(uAttrListSize))
    };
    InitializeProcThreadAttributeList(vSix.lpAttributeList, 1, 0, &uAttrListSize);
    HANDLE ahInherit[] {x_hFile, reinterpret_cast<HANDLE>(x_hSocket), x_hEvent, x_hMapping, x_hMutex};
    UpdateProcThreadAttribute(
        vSix.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        ahInherit, sizeof(ahInherit), nullptr, nullptr
    );
    Format(
        L"UchFile.exe + %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIuPTR)
                L" %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIu64),
        reinterpret_cast<UPtr>(x_hMapping),
        reinterpret_cast<UPtr>(x_hMutex),
        reinterpret_cast<UPtr>(x_hFile),
        static_cast<UPtr>(x_hSocket),
        reinterpret_cast<UPtr>(x_hEvent),
        e.uSize
    );
    auto dwbRes = CreateProcessW(
        Ucl::Ucf().c_str(), g_szWideBuf,
        nullptr, nullptr, true,
        CREATE_NEW_CONSOLE | HIGH_PRIORITY_CLASS | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr,
        &vSix.StartupInfo, &x_vPi
    );
    if (dwbRes) {
        CloseHandle(std::exchange(x_hFile, INVALID_HANDLE_VALUE));
        closesocket(std::exchange(x_hSocket, INVALID_SOCKET));
        WaitForSingleObject(x_hEvent, INFINITE);
        CloseHandle(std::exchange(x_hEvent, INVALID_HANDLE_VALUE));
        Ucl::Iog().RegisterTick(*this);
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId,
            true,
            vSn.GetPort()
        });
    }
    else {
        msgbox mbx {*this, x_su8MbxTitle};
        mbx.icon(msgbox::icon_error);
        mbx << L"Failed to start the file transmitting process";
        mbx();
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId,
            false,
            vSn.GetPort()
        });
        x_bAskCancel = false;
        close();
    }
}

void FrmFileRecv::OnEvent(protocol::EvpFileCancel &e) noexcept {
    if (e.uId != x_uId)
        return;
    Ucl::Iog().PostJob([this] {
        RAII_LOCK(x_mtx);
        msgbox mbx {*this, x_su8MbxTitle};
        mbx.icon(msgbox::icon_information);
        mbx << L"[" << x_pPipl->GetUser() << L"] has canceled the transmition";
        mbx();
        x_bAskCancel = false;
        close();
    });
}

void FrmFileRecv::X_OnDestroy(const arg_destroy &e) {
    Ucl::Iog().UnregisterTick(*this);
    Ucl::Bus().Unregister(*this);
    if (x_vPi.hProcess != INVALID_HANDLE_VALUE) {
        if (x_bCanceling)
            TerminateProcess(x_vPi.hProcess, EXIT_FAILURE);
        HANDLE ahWait[] {x_vPi.hProcess, x_vPi.hThread};
        WaitForMultipleObjects(2, ahWait, TRUE, INFINITE);
        CloseHandle(x_vPi.hProcess);
        CloseHandle(x_vPi.hThread);
    }
    if (x_hFile != INVALID_HANDLE_VALUE)
        CloseHandle(x_hFile);
    if (x_hSocket != INVALID_SOCKET)
        closesocket(x_hSocket);
    if (x_hEvent != INVALID_HANDLE_VALUE)
        CloseHandle(x_hEvent);
    if (x_hMapping != INVALID_HANDLE_VALUE)
        CloseHandle(x_hMapping);
    if (x_hMutex != INVALID_HANDLE_VALUE)
        CloseHandle(x_hMutex);
    if (x_vPi.hProcess != INVALID_HANDLE_VALUE && !x_bCanceling) {
        Ucl::Bus().PostEvent(event::EvMessage {
            kszCatFile, x_pPipl->GetUser(), kszSelf,
                x_sFilePath + L" [" + FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B") + L"]"
        });
    }
}

void FrmFileRecv::X_OnUser() {
    if (x_atmbUpd.test_and_set()) {
        // lag
        return;
    }
    auto usNow = GetTimeStamp();
    if (!StampDue(usNow, x_usNextUpd)) {
        x_atmbUpd.clear();
        return;
    }
    x_usNextUpd = usNow + 1'000'000;
    constexpr static uchfile::IpcData vDone {~0, ~0, ~0};
    WaitForSingleObject(x_hMutex, INFINITE);
    auto pIpc = reinterpret_cast<const uchfile::IpcData *>(
        MapViewOfFile(x_hMapping, FILE_MAP_READ, 0, 0, sizeof(uchfile::IpcData))
    );
    auto vIpc = *pIpc;
    UnmapViewOfFile(pIpc);
    ReleaseMutex(x_hMutex);
    if (!std::memcmp(&vIpc, &vDone, sizeof(uchfile::IpcData))) {
        enabled(false);
        x_lblProg.caption(
            FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B") + L" / " +
            FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B")
        );
        x_pgbProg.value(x_kucBarAmount);
        x_lblState.caption(L"Completed, cleaning up...");
        x_bAskCancel = false;
        x_bCanceling = false;
        close();
    }
    else {
        x_lblProg.caption(
            FormatSize(vIpc.uzFile, L"GiB", L"MiB", L"KiB", L"B") + L" / " +
            FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B")
        );
        x_pgbProg.value(static_cast<U32>(x_kucBarAmount * vIpc.uzFile / x_uzFileSize));
        x_lblState.caption(FormatString(L"Receiving...\nUpload = %s\nDownload = %s",
            FormatSize(vIpc.uzSentSec, L"GiB/s", L"MiB/s", L"KiB/s", L"B/s").c_str(),
            FormatSize(vIpc.uzRcvdSec, L"GiB/s", L"MiB/s", L"KiB/s", L"B/s").c_str()
        ));
    }
    x_atmbUpd.clear();
}

bool FrmFileRecv::OnTick(U64 usNow) noexcept {
    user(nullptr);
    return true;
}
