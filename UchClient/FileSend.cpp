#include "Common.hpp"

#include "FileSend.hpp"
#include "Ucl.hpp"

#include <nana/gui.hpp>

FileSend::FileSend(UccPipl *pPipl, const String &sPath) :
    x_pPipl(pPipl), x_sName(sPath.substr(sPath.rfind(L'\\') + 1))
{
    Ucl::Bus().Register(*this);
    x_hFile = CreateFileHandleInherit(
        sPath, GENERIC_READ, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
    );
    x_hSocket = CreateBoundSocket(MakeSockName(Ucl::Cfg()[UclCfg::kszLisHost], L"0"), false);
    x_pPipl->PostPacket(protocol::EvpFileReq {
        reinterpret_cast<U64>(this),
        x_sName,
        GetFileSize(x_hFile),
        GetLocalSockName(x_hSocket).GetPort()
    });
}

FileSend::~FileSend() {
    Ucl::Bus().Unregister(*this);
    if (x_hFile != INVALID_HANDLE_VALUE)
        CloseHandle(x_hFile);
    if (x_hSocket != INVALID_SOCKET)
        closesocket(x_hSocket);
}

void FileSend::OnEvent(protocol::EvpFileRes &e) noexcept {
    if (e.uId != reinterpret_cast<U64>(this))
        return;
    Ucl::Iog().PostJob([this, e] {
        using namespace nana;
        if (!e.bAccepted) {
            msgbox mbx {u8"Uch - Send file"};
            mbx.icon(msgbox::icon_information);
            mbx << L"File request refused";
            mbx();
            delete this;
            return;
        }
        auto vSn = x_pPipl->GetLower().GetRemoteSockName();
        vSn.SetPort(e.uPort);
        ::connect(x_hSocket, &vSn.vSockAddr, vSn.nSockLen);
        auto uLen = Format(
            L"UchFile.exe - %p %p",
            reinterpret_cast<void *>(x_hFile),
            reinterpret_cast<void *>(x_hSocket)
        );
        std::vector<wchar_t> vecCmdLine {g_szWideBuf, g_szWideBuf + uLen + 1};
        uLen = Format(L"Uch - Sending [%s] to [%s]...", x_sName.c_str(), x_pPipl->GetUser().c_str());
        std::vector<wchar_t> vecTitle {g_szWideBuf, g_szWideBuf + uLen + 1};
        STARTUPINFOW vSi {static_cast<DWORD>(sizeof(STARTUPINFOW))};
        vSi.lpTitle = vecTitle.data();
        PROCESS_INFORMATION vPi;
        if (CreateProcessW(
            Ucl::Ucf().c_str(), vecCmdLine.data(),
            nullptr, nullptr, true,
            CREATE_NEW_CONSOLE | HIGH_PRIORITY_CLASS,
            nullptr, nullptr,
            &vSi, &vPi
        ))
        {
            CloseHandle(vPi.hProcess);
            CloseHandle(vPi.hThread);
        }
        // else? else what?
        delete this;
    });
}
