#include "Common.hpp"

#include "FrmRegister.hpp"
#include "Ucl.hpp"

#include <cryptopp/sha.h>

using namespace nana;

FrmRegister::FrmRegister(const form &frmParent) :
    form(frmParent, {360, 200}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch - Register");
    events().unload(std::bind(&FrmRegister::X_OnDestroy, this, std::placeholders::_1));
    x_btnRegister.caption(L"Register");
    x_btnRegister.events().click(std::bind(&FrmRegister::X_OnRegister, this));
    x_btnRegister.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmRegister::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_txtUsername.multi_lines(false).tip_string(u8"Username:");
    x_txtUsername.events().focus([this] (const arg_focus &e) {
        x_txtUsername.select(e.getting);
    });
    x_txtUsername.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_txtPassword.events().focus([this] (const arg_focus &e) {
        x_txtPassword.select(e.getting);
    });
    x_txtPassword.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_txtQuestion.multi_lines(false).tip_string(u8"Question:");
    x_txtQuestion.events().focus([this] (const arg_focus &e) {
        x_txtQuestion.select(e.getting);
    });
    x_txtQuestion.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_txtAnswer.multi_lines(false).tip_string(u8"Answer:");
    x_txtAnswer.events().focus([this] (const arg_focus &e) {
        x_txtAnswer.select(e.getting);
    });
    x_txtAnswer.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_pl.div(
        "<> <weight=190 vert"
        "   <> <weight=153 vert"
        "       <weight=25 User> <>"
        "       <weight=25 Pass> <>"
        "       <weight=25 Ques> <>"
        "       <weight=25 Answ> <>"
        "       <weight=25 gap=8 Btns>"
        "   > <>"
        "> <>"
    );
    x_pl["User"] << x_txtUsername;
    x_pl["Pass"] << x_txtPassword;
    x_pl["Ques"] << x_txtQuestion;
    x_pl["Answ"] << x_txtAnswer;
    x_pl["Btns"] << x_btnRegister << x_btnCancel;
    x_pl.collocate();
    Ucl::Bus().Register(*this);
}


void FrmRegister::OnEvent(protocol::EvsRegisRes &e) noexcept {
    if (!e.bSuccess) {
        msgbox mbx {*this, u8"Uch - Register", nana::msgbox::ok};
        mbx.icon(msgbox::icon_error);
        mbx << L"Failed to register: " << e.sResult;
        mbx();
        enabled(true);
        return;
    }
    close();
}

void FrmRegister::X_OnRegister() {
    enabled(false);
    ShaDigest vPass, vAnsw;
    CryptoPP::SHA256 vSha;
    auto uPass = ConvertWideToUtf8(x_txtPassword.caption_wstring());
    vSha.CalculateDigest(
        reinterpret_cast<CryptoPP::byte *>(&vPass),
        reinterpret_cast<CryptoPP::byte *>(g_szUtf8Buf),
        uPass
    );
    auto uAnsw = ConvertWideToUtf8(x_txtAnswer.caption_wstring());
    vSha.CalculateDigest(
        reinterpret_cast<CryptoPP::byte *>(&vAnsw),
        reinterpret_cast<CryptoPP::byte *>(g_szUtf8Buf),
        uAnsw
    );
    Ucl::Con()->PostPacket(protocol::EvcRegisReq {
        x_txtUsername.caption_wstring(),
        std::move(vPass),
        x_txtQuestion.caption_wstring(),
        std::move(vAnsw)
    });
}

void FrmRegister::X_OnDestroy(const nana::arg_unload &e) {
    UNREFERENCED_PARAMETER(e);
    Ucl::Bus().Unregister(*this);
}
