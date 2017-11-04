#include "Common.hpp"

#include "Event.hpp"
#include "FileRecv.hpp"
#include "Ucl.hpp"
#include "UccPipl.hpp"

#include <nana/gui.hpp>
#include <nana/gui/filebox.hpp>

void FileRecv::Run(UccPipl *pPipl, const protocol::EvpFileReq &e) {
    using namespace nana;
    msgbox mbx {nullptr, u8"Uch - Receive file", msgbox::yes_no};
    mbx.icon(msgbox::icon_question);
    mbx << e.sName << L" [" << FormatSize(e.uSize, L"GiB", L"MiB", L"KiB", L"B") << L"]";
    mbx << L"Receive the file from [" << pPipl->GetUser() << L"]?\n";
    if (mbx() != msgbox::pick_yes) {
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId, false, 0
        });
        return;
    }
    filebox fbx {nullptr, false};
    fbx.init_file(AsUtf8String(e.sName));
    if (!fbx()) {
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId, false, 0
        });
        return;
    }
    auto sPath = AsWideString(fbx.file());
    auto hFile = CreateFileHandleInherit(
        sPath, GENERIC_WRITE, CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
    );
    auto hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszLisHost], L"0"), false);
    auto vSn = pPipl->GetLower().GetRemoteSockName();
    vSn.SetPort(e.uPort);
    ::connect(hSocket, &vSn.vSockAddr, vSn.nSockLen);
    vSn = GetLocalSockName(hSocket);
    SECURITY_ATTRIBUTES vSecAttr {static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)), nullptr, true};
    auto hEvent = CreateEventW(&vSecAttr, TRUE, FALSE, nullptr);
    auto uLen = Format(
        L"UchFile.exe + %p %p %p %I64u",
        reinterpret_cast<void *>(hFile),
        reinterpret_cast<void *>(hSocket),
        reinterpret_cast<void *>(hEvent),
        e.uSize
    );
    std::vector<wchar_t> vecCmdLine {g_szWideBuf, g_szWideBuf + uLen + 1};
    uLen = Format(L"Uch - Receiving [%s] from [%s]...", e.sName.c_str(), pPipl->GetUser().c_str());
    std::vector<wchar_t> vecTitle {g_szWideBuf, g_szWideBuf + uLen + 1};
    USize uAttrListSize;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &uAttrListSize);
    STARTUPINFOEXW vSix {
        {static_cast<DWORD>(sizeof(STARTUPINFOEXW))},
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(::operator new(uAttrListSize))
    };
    InitializeProcThreadAttributeList(vSix.lpAttributeList, 1, 0, &uAttrListSize);
    HANDLE ahInherit[3] {hFile, reinterpret_cast<HANDLE>(hSocket), hEvent};
    UpdateProcThreadAttribute(
        vSix.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        ahInherit, sizeof(ahInherit), nullptr, nullptr
    );
    vSix.StartupInfo.lpTitle = vecTitle.data();
    PROCESS_INFORMATION vPi;
    if (CreateProcessW(
        Ucl::Ucf().c_str(), vecCmdLine.data(),
        nullptr, nullptr, true,
        CREATE_NEW_CONSOLE | HIGH_PRIORITY_CLASS | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr,
        &vSix.StartupInfo, &vPi
    ))
    {
        WaitForSingleObject(hEvent, INFINITE);
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId,
            true,
            vSn.GetPort()
        });
        CloseHandle(vPi.hProcess);
        CloseHandle(vPi.hThread);
        CloseHandle(hEvent);
        CloseHandle(hFile);
        closesocket(hSocket);
    }
    else {
        CloseHandle(hEvent);
        CloseHandle(hFile);
        closesocket(hSocket);
        pPipl->PostPacket(protocol::EvpFileRes {
            e.uId,
            false,
            vSn.GetPort()
        });
    }
}
