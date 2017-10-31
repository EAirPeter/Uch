#include "Common.hpp"

#include "FrmRecoPass.hpp"
#include "Ucl.hpp"

#include <cryptopp/sha.h>

using namespace nana;

FrmRecoPass::FrmRecoPass(const nana::form &frmParent, const String &sUser, const String &sQues) :
    form(frmParent, {360, 200}, appear::decorate<appear::taskbar, appear::minimize>()),
    x_sUser(sUser)
{
    caption(L"Uch-Password Recovery");
    events().unload(std::bind(&FrmRecoPass::X_OnDestroy, this, std::placeholders::_1));
    x_btnNext.caption(L"Next");
    x_btnNext.events().click(std::bind(&FrmRecoPass::X_OnNext, this));
    x_btnNext.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnNext();
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmRecoPass::close, this));
    x_btnCancel.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lblUsername.caption(String {L"Username: "} + sUser);
    x_lblQuestion.caption(String {L"Please answer: " + sQues});
    x_txtAnswer.multi_lines(false).tip_string(u8"Answer:");
    x_txtAnswer.events().focus([this] (const arg_focus &e) {
        x_txtAnswer.select(e.getting);
    });
    x_txtAnswer.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnNext();
    });
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_txtPassword.events().focus([this] (const arg_focus &e) {
        x_txtPassword.select(e.getting);
    });
    x_txtPassword.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnNext();
    });
    x_pl.div(
        "<> <weight=190 vert"
        "   <> <weight=153 vert"
        "       <weight=25 User> <>"
        "       <weight=25 Ques> <>"
        "       <weight=25 Answ> <>"
        "       <weight=25 Pass> <>"
        "       <weight=25 gap=8 Btns>"
        "   > <>"
        "> <>"
    );
    x_pl["User"] << x_lblUsername;
    x_pl["Ques"] << x_lblQuestion;
    x_pl["Answ"] << x_txtAnswer;
    x_pl["Pass"] << x_txtPassword;
    x_pl["Btns"] << x_btnNext << x_btnCancel;
    x_pl.collocate();
    Ucl::Bus().Register(*this);
}

void FrmRecoPass::OnEvent(protocol::EvsRecoPassRes &e) noexcept {
    if (!e.bSuccess) {
        msgbox mbx {*this, u8"Uch - Recover password", nana::msgbox::ok};
        mbx.icon(msgbox::icon_error);
        mbx << L"Failed to recover your password: " << e.sResult;
        mbx();
        enabled(true);
        return;
    }
    close();
}

void FrmRecoPass::X_OnNext(){
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
    Ucl::Con()->PostPacket(protocol::EvcRecoPassReq {
        x_sUser,
        std::move(vAnsw),
        std::move(vPass)
    });
}

void FrmRecoPass::X_OnDestroy(const nana::arg_unload &e) {
    UNREFERENCED_PARAMETER(e);
    Ucl::Bus().Unregister(*this);
}
