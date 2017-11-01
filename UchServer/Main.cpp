#include "Common.hpp"

#include "../UchCommon/System.hpp"

#include "FrmMain.hpp"
#include "Usv.hpp"

#include <nana/gui.hpp>

Usv Usv::x_vInstance {};

int wmain() {
    try {
        System::GlobalStartup();
        Usv::Cfg().Load();
        Usv::Cfg().Save();
        Usv::Sto().Load();
        Usv::Iog().Start();
        Usv::Cmg() = std::make_unique<ClientManager>();
        FrmMain frmMain;
        frmMain.show();
        nana::exec();
        Usv::Cmg()->Wait();
        Usv::Iog().Shutdown();
        Usv::Sto().Save();
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