#include "Common.hpp"

#include "FrmRecoPass.hpp"
#include "FrmRecoUser.hpp"
#include "Ucl.hpp"

using namespace nana;

FrmRecoUser::FrmRecoUser(const nana::form &frmParent) :
    form(frmParent, {360, 200}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch - Recover your password");
    events().unload(std::bind(&FrmRecoUser::X_OnDestroy, this, std::placeholders::_1));
    x_btnNext.caption(L"Next");
    x_btnNext.events().click(std::bind(&FrmRecoUser::X_OnNext, this));
    x_btnNext.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnNext();
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmRecoUser::close, this));
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
            X_OnNext();
    });
    x_pl.div(
        "<> <weight=190 vert"
        "   <> <weight=57 vert"
        "       <weight=25 User> <>"
        "       <weight=25 gap=8 Btns>"
        "   > <>"
        "> <>"
    );
    x_pl["User"] << x_txtUsername;
    x_pl["Btns"] << x_btnNext << x_btnCancel;
    x_pl.collocate();
    Ucl::Bus().Register(*this);
}

void FrmRecoUser::OnEvent(protocol::EvsRecoUserRes &e) noexcept {
    Ucl::Iog().PostJob([this, e] {
        if (!e.bSuccess) {
            msgbox mbx {*this, u8"Uch - Recover password", nana::msgbox::ok};
            mbx.icon(msgbox::icon_error);
            mbx << L"Failed to recover your password: " << e.sResult;
            mbx();
            enabled(true);
            return;
        }
        FrmRecoPass frmRecoPass {*this, x_txtUsername.caption_wstring(), e.sResult};
        hide();
        frmRecoPass.modality();
        close();
    });
}

void FrmRecoUser::X_OnNext() {
    enabled(false);
    Ucl::Con()->PostPacket(protocol::EvcRecoUserReq {
        x_txtUsername.caption_wstring()
    });
}

void FrmRecoUser::X_OnDestroy(const nana::arg_unload &e) {
    UNREFERENCED_PARAMETER(e);
    Ucl::Bus().Unregister(*this);
}
