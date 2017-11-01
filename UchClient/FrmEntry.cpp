#include "Common.hpp"

#include "FrmEntry.hpp"
#include "FrmMain.hpp"
#include "FrmRecoUser.hpp"
#include "FrmRegister.hpp"
#include "Ucl.hpp"

#include <cryptopp/sha.h>

using namespace nana;

FrmEntry::FrmEntry() :
    form(API::make_center(360, 200), appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch - Login");
    events().unload(std::bind(&FrmEntry::X_OnDestroy, this, std::placeholders::_1));
    x_btnLogin.caption(L"Login");
    x_btnLogin.events().click(std::bind(&FrmEntry::X_OnLogin, this));
    x_btnLogin.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnLogin();
    });
    x_btnExit.caption(L"Exit");
    x_btnExit.events().click(std::bind(&FrmEntry::close, this));
    x_btnExit.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_btnRegister.caption(L"Register");
    x_btnRegister.events().click(std::bind(&FrmEntry::X_OnRegister, this));
    x_btnRegister.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_btnForget.caption(L"Forget?");
    x_btnForget.events().click(std::bind(&FrmEntry::X_OnForget, this));
    x_btnForget.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnForget();
    });
    x_txtUsername.multi_lines(false).tip_string(u8"Username:");
    x_txtUsername.events().focus([this] (const arg_focus &e) {
        x_txtUsername.select(e.getting);
    });
    x_txtUsername.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnLogin();
    });
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_txtPassword.events().focus([this] (const arg_focus &e) {
        x_txtPassword.select(e.getting);
    });
    x_txtPassword.events().key_press([this] (const arg_keyboard &e) {
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
    x_pl["Btns"] << x_btnLogin << x_btnExit;
    x_pl.collocate();
    Ucl::Bus().Register(*this);
}

void FrmEntry::OnEvent(protocol::EvsLoginRes &e) noexcept {
    Ucl::Iog().PostJob([this, e] {
        if (!e.bSuccess) {
            msgbox mbx {*this, u8"Uch - Login", nana::msgbox::ok};
            mbx.icon(msgbox::icon_error);
            mbx << L"Failed to login: " << e.sResult;
            mbx();
            return;
        }
        Ucl::Usr() = x_txtUsername.caption_wstring();
        FrmMain frmMain {*this};
        for (auto &vMsg : e.vecMsgFf) {
            Ucl::Bus().PostEvent(event::EvMessage {protocol::ChatMessage {
                String {L"(Offline) "} +std::move(vMsg.sFrom),
                std::move(vMsg.sMessage)
            }});
        }
        Ucl::Pmg()->SetFfline(e.vecUsrFf);
        Ucl::Pmg()->SetOnline(e.vecUsrOn);
        hide();
        frmMain.modality();
        close();
    });
}

void FrmEntry::X_OnLogin() {
    auto uPass = ConvertWideToUtf8(x_txtPassword.caption_wstring());
    ShaDigest vDigest;
    CryptoPP::SHA256 {}.CalculateDigest(
        reinterpret_cast<CryptoPP::byte *>(&vDigest),
        reinterpret_cast<CryptoPP::byte *>(g_szUtf8Buf),
        uPass
    );
    Ucl::Con()->PostPacket(protocol::EvcLoginReq {
        protocol::OnlineUser {
            x_txtUsername.caption_wstring(),
            Ucl::Pmg()->GetListenSockName()
        },
        std::move(vDigest)
    });
}

void FrmEntry::X_OnRegister() {
    enabled(false);
    FrmRegister frmRegister {*this};
    frmRegister.modality();
    enabled(true);
}

void FrmEntry::X_OnForget() {
    enabled(false);
    FrmRecoUser frmRecoUser {*this};
    frmRecoUser.modality();
    enabled(true);
}

void FrmEntry::X_OnDestroy(const nana::arg_unload &e) {
    UNREFERENCED_PARAMETER(e);
    Ucl::Bus().Unregister(*this);
    Ucl::Con()->Shutdown();
    Ucl::Pmg()->Shutdown();
}
