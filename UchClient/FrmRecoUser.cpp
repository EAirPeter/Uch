#include "Common.hpp"

#include "FrmRecoPass.hpp"
#include "FrmRecoUser.hpp"

using namespace nana;

FrmRecoUser::FrmRecoUser(const nana::form &frmParent) :
    form(frmParent, {360, 200}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch-Password Recovery");
    x_btnNext.caption(L"Next");
    x_btnNext.events().click(std::bind(&FrmRecoUser::X_OnNext, this));
    x_btnNext.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnNext();
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmRecoUser::X_OnCancel, this));
    x_btnCancel.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnCancel();
    });
    x_txtUsername.multi_lines(false).tip_string(u8"Username:");
    x_txtUsername.events().focus.connect([&] (auto &&e) {
        x_txtUsername.select(e.getting);
    });
    x_txtUsername.events().key_press.connect([&] (auto &&e) {
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
}

void FrmRecoUser::X_OnNext() {
    wprintf(L"--reco-user\n");
    FrmRecoPass frmRecoPass {*this, x_txtUsername.caption_wstring(), L"mmp?"};
    frmRecoPass.modality();
    close();
}

void FrmRecoUser::X_OnCancel() {
    close();
}
