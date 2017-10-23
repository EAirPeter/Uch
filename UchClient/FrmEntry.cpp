#include "Common.hpp"

#include "FrmEntry.hpp"

using namespace nana;

FrmEntry::FrmEntry() :
    form(API::make_center(360, 200), appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch-Login");
    x_btnLogin.caption(L"Login");
    x_btnQuit.caption(L"Quit");
    x_btnRegister.caption(L"Register");
    x_btnForget.caption(L"Forget?");
    x_txtServerHost.multi_lines(false).tip_string(u8"Server:").caption(L"localhost").register_shortkey(L'\r');
    x_txtServerHost.events().focus([&] (const nana::arg_focus &) { x_txtServerHost.select(true); });
    x_txtServerHost.events().shortkey([&] (const nana::arg_keyboard &e) { if (e.key) X_OnLogin(); });
    x_txtServerPort.multi_lines(false).tip_string(u8"Port:").caption(L"54289");
    x_txtUsername.multi_lines(false).tip_string(u8"Username:");
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_pl.div(
        "<> <weight=80% vert"
        "   <> <weight=70% vert"
        "       <weight=25 arrange=[70%,variable] gap=7 Addr> <>"
        "       <weight=25 arrange=[70%,variable] gap=7 User> <>"
        "       <weight=25 arrange=[70%,variable] gap=7 Pass> <>"
        "       <weight=25 gap=7 Btns>"
        "   > <>"
        "> <>"
    );
    x_pl["Addr"] << x_txtServerHost << x_txtServerPort;
    x_pl["User"] << x_txtUsername << x_btnRegister;
    x_pl["Pass"] << x_txtPassword << x_btnForget;
    x_pl["Btns"] << x_btnLogin << x_btnQuit;
    x_pl.collocate();
    x_txtServerHost.focus();
}

void FrmEntry::X_OnLogin() {
    auto sHost = x_txtServerHost.caption_wstring();
    auto sPort = x_txtServerPort.caption_wstring();
    auto sUser = x_txtUsername.caption_wstring();
    auto sPass = x_txtPassword.caption_wstring();
    wprintf(L"%s\n%s\n%s\n%s\n", sHost.c_str(), sPort.c_str(), sUser.c_str(), sPass.c_str());
}
