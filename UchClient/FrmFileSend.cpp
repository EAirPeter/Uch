#include "Common.hpp"

#include "../UchFile/IpcData.hpp"

#include "Event.hpp"
#include "FrmFileSend.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmFileSend::FrmFileSend(const nana::form &frmParent, UccPipl *pPipl, const String &sPath) :
    form(frmParent, {480, 240}, appear::decorate<appear::taskbar, appear::minimize>()),
    x_pPipl(pPipl), x_sFileName(sPath.substr(sPath.rfind(L'\\') + 1)), x_sFilePath(sPath),
    x_su8MbxTitle(TitleU8(FormatString(L"Send [%s] to [%s]", x_sFileName.c_str(), pPipl->GetUser().c_str())))
{
    Ucl::Bus().Register(*this);
    caption(Title(FormatString(L"Sending [%s] to [%s]...", x_sFileName.c_str(), pPipl->GetUser().c_str())));
    events().destroy(std::bind(&FrmFileSend::X_OnDestroy, this, std::placeholders::_1));
    events().user(std::bind(&FrmFileSend::X_OnUser, this));
    events().unload([this] (const arg_unload &e) {
        if (x_bAskCancel) {
            msgbox mbx {*this, x_su8MbxTitle, msgbox::yes_no};
            mbx.icon(msgbox::icon_question);
            mbx << L"Are you sure to cancel?";
            if (mbx() != mbx.pick_yes) {
                e.cancel = true;
                return;
            }
            x_pPipl->PostPacket(protocol::EvpFileCancel {reinterpret_cast<U64>(this)});
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
        "   <Stat> <weight=7>"
        "   <weight=25 <> <weight=81 Canc>>"
    );
    x_pl["Name"] << x_lblName;
    x_pl["Plbl"] << x_lblProg;
    x_pl["Pbar"] << x_pgbProg;
    x_pl["Stat"] << x_lblState;
    x_pl["Canc"] << x_btnCancel;
    x_pl.collocate();
    x_hFile = CreateFileHandleInherit(
        sPath, GENERIC_READ, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
    );
    x_uzFileSize = GetFileSize(x_hFile);
    x_hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszLisHost], L"0"), false);
    x_pPipl->PostPacket(protocol::EvpFileReq {
        reinterpret_cast<U64>(this),
        x_sFileName,
        GetFileSize(x_hFile),
        GetLocalSockName(x_hSocket).GetPort()
    });
}

void FrmFileSend::OnEvent(protocol::EvpFileRes &e) noexcept {
    if (e.uId != reinterpret_cast<U64>(this))
        return;
    Ucl::Iog().PostJob([this, e] {
        if (!e.bAccepted) {
            msgbox mbx {*this, x_su8MbxTitle};
            mbx.icon(msgbox::icon_information);
            mbx << L"[" << x_pPipl->GetUser() << "] has refused the transmition";
            mbx();
            x_bAskCancel = false;
            close();
            return;
        }
        auto vSn = x_pPipl->GetLower().GetRemoteSockName();
        vSn.SetPort(e.uPort);
        ::connect(x_hSocket, &vSn.vSockAddr, vSn.nSockLen);
        SECURITY_ATTRIBUTES vSecAttr {static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)), nullptr, true};
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
        HANDLE ahInherit[] {x_hFile, reinterpret_cast<HANDLE>(x_hSocket), x_hMapping, x_hMutex};
        UpdateProcThreadAttribute(
            vSix.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            ahInherit, sizeof(ahInherit), nullptr, nullptr
        );
        Format(
            L"UchFile.exe - %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIuPTR)
                L" %" CONCAT(L, PRIuPTR) L" %" CONCAT(L, PRIuPTR),
            reinterpret_cast<UPtr>(x_hMapping),
            reinterpret_cast<UPtr>(x_hMutex),
            reinterpret_cast<UPtr>(x_hFile),
            static_cast<UPtr>(x_hSocket)
        );
        auto dwbRes = CreateProcessW(
            Ucl::Ucf().c_str(), g_szWideBuf,
            nullptr, nullptr, true,
            HIGH_PRIORITY_CLASS | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr,
            &vSix.StartupInfo, &x_vPi
        );
        DeleteProcThreadAttributeList(vSix.lpAttributeList);
        ::operator delete(vSix.lpAttributeList);
        if (dwbRes) {
            CloseHandle(std::exchange(x_hFile, INVALID_HANDLE_VALUE));
            closesocket(std::exchange(x_hSocket, INVALID_SOCKET));
            Ucl::Iog().RegisterTick(*this);
        }
        else {
            msgbox mbx {*this, x_su8MbxTitle};
            mbx.icon(msgbox::icon_error);
            mbx << L"Failed to start the file transmitting process";
            mbx();
            x_pPipl->PostPacket(protocol::EvpFileCancel {reinterpret_cast<U64>(this)});
            x_bAskCancel = false;
            close();
        }
    });
}

void FrmFileSend::OnEvent(protocol::EvpFileCancel &e) noexcept {
    if (e.uId != reinterpret_cast<U64>(this))
        return;
    Ucl::Iog().PostJob([this] {
        msgbox mbx {*this, x_su8MbxTitle};
        mbx.icon(msgbox::icon_information);
        mbx << L"[" << x_pPipl->GetUser() << L"] has canceled the transmition";
        mbx();
        x_bAskCancel = false;
        close();
    });
}

void FrmFileSend::X_OnDestroy(const arg_destroy &e) {
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
    if (x_hMapping != INVALID_HANDLE_VALUE)
        CloseHandle(x_hMapping);
    if (x_hMutex != INVALID_HANDLE_VALUE)
        CloseHandle(x_hMutex);
    if (x_vPi.hProcess != INVALID_HANDLE_VALUE && !x_bCanceling) {
        Ucl::Bus().PostEvent(event::EvMessage {
            kszCatFile, kszSelf, x_pPipl->GetUser(),
                x_sFilePath + L" [" + FormatSize(x_uzFileSize, L"GiB", L"MiB", L"KiB", L"B") + L"]"
        });
    }
}

void FrmFileSend::X_OnUser() {
    if (x_atmbUpd.test_and_set()) {
        // lag
        return;
    }
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
        x_lblState.caption(FormatString(L"Sending...\nUpload = %s\nDownload = %s",
            FormatSize(vIpc.uzSentSec, L"GiB/s", L"MiB/s", L"KiB/s", L"B/s").c_str(),
            FormatSize(vIpc.uzRcvdSec, L"GiB/s", L"MiB/s", L"KiB/s", L"B/s").c_str()
        ));
    }
    x_atmbUpd.clear();
}

bool FrmFileSend::OnTick(U64 usNow) noexcept {
    user(nullptr);
    return true;
}
