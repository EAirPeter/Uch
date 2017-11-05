#include "Common.hpp"

#include "../UchCommon/System.hpp"

#include "FrmEntry.hpp"
#include "FrmMain.hpp"
#include "Ucl.hpp"

#include <nana/gui.hpp>

Ucl Ucl::x_vInstance {};

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        System::GlobalStartup();
        auto dwb = GetModuleFileNameW(nullptr, g_szWideBuf, STRCVT_BUFSIZE);
        auto sAppName = String {g_szWideBuf, dwb};
        sAppName.resize(sAppName.rfind(L'\\'));
        Ucl::Ucf() = sAppName + L"\\UchFile.exe";
        Ucl::Cfg().Load();
        Ucl::Cfg().Save();
        Ucl::Iog().Start();
        Ucl::Pmg() = std::make_unique<PeerManager>();
        Ucl::Con() = std::make_unique<UclPipl>();
        FrmEntry frmEntry;
        frmEntry.show();
        nana::exec();
        Ucl::Con()->Wait();
        Ucl::Pmg()->Wait();
        Ucl::Iog().Shutdown();
    }
    catch (ExnSys &e) {
        nana::msgbox mbx {u8"Uch - Fatal error"};
        mbx.icon(nana::msgbox::icon_error);
        mbx << L"Unexpected system error: " << e.dwError;
        mbx();
        ExitProcess(static_cast<UINT>(e.dwError));
    }
    catch (ExnWsa &e) {
        nana::msgbox mbx {u8"Uch - Fatal error"};
        mbx.icon(nana::msgbox::icon_error);
        mbx << L"Unexpected winsock error: " << e.nError;
        mbx();
        ExitProcess(static_cast<UINT>(e.nError));
    }
    return 0;
}
