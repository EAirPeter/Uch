#include "Common.hpp"

#include "FrmEntry.hpp"
#include "FrmMain.hpp"
#include "FrmRecoUser.hpp"
#include "FrmRegister.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmEntry::FrmEntry() :
    form(API::make_center(360, 200), appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch-Login");
    x_btnLogin.caption(L"Login");
    x_btnLogin.events().click(std::bind(&FrmEntry::X_OnLogin, this));
    x_btnLogin.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnLogin();
    });
    x_btnQuit.caption(L"Quit");
    x_btnQuit.events().click(std::bind(&FrmEntry::X_OnQuit, this));
    x_btnQuit.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnQuit();
    });
    x_btnRegister.caption(L"Register");
    x_btnRegister.events().click(std::bind(&FrmEntry::X_OnRegister, this));
    x_btnRegister.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_btnForget.caption(L"Forget?");
    x_btnForget.events().click(std::bind(&FrmEntry::X_OnForget, this));
    x_btnForget.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnForget();
    });
    x_txtUsername.multi_lines(false).tip_string(u8"Username:");
    x_txtUsername.events().focus.connect([&] (auto &&e) {
        x_txtUsername.select(e.getting);
    });
    x_txtUsername.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnLogin();
    });
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_txtPassword.events().focus.connect([&] (auto &&e) {
        x_txtPassword.select(e.getting);
    });
    x_txtPassword.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnLogin();
    });
    x_pl.div(
        "<> <weight=280 vert"
        "   <> <weight=89 vert"
        "       <weight=25 arrange=[190,variable] gap=8 User> <>"
        "       <weight=25 arrange=[190,variable] gap=8 Pass> <>"
        "       <weight=25 gap=8 Btns>"
        "   > <>"
        "> <>"
    );
    x_pl["User"] << x_txtUsername << x_btnRegister;
    x_pl["Pass"] << x_txtPassword << x_btnForget;
    x_pl["Btns"] << x_btnLogin << x_btnQuit;
    x_pl.collocate();
}

void FrmEntry::X_OnLogin() {
    auto sUser = x_txtUsername.caption_wstring();
    auto sPass = x_txtPassword.caption_wstring();
    wprintf(L"login: %s %s\n", sUser.c_str(), sPass.c_str());
    Ucl::Usr() = sUser;
    FrmMain frmMain {*this};
    frmMain.modality();
    close();
}

void FrmEntry::X_OnQuit() {
    wprintf(L"quit\n");
    close();
}

void FrmEntry::X_OnRegister() {
    wprintf(L"register\n");
    FrmRegister frmRegister {*this};
    frmRegister.modality();
}

void FrmEntry::X_OnForget() {
    wprintf(L"forget\n");
    FrmRecoUser frmRecoUser {*this};
    frmRecoUser.modality();
}
