#include "Common.hpp"

#include "FrmRegister.hpp"

using namespace nana;

FrmRegister::FrmRegister(const form &frmParent) :
    form(frmParent, {360, 200}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(L"Uch-Register");
    x_btnRegister.caption(L"Register");
    x_btnRegister.events().click(std::bind(&FrmRegister::X_OnRegister, this));
    x_btnRegister.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmRegister::X_OnCancel, this));
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
            X_OnRegister();
    });
    x_txtPassword.multi_lines(false).tip_string(u8"Password:").mask(L'*');
    x_txtPassword.events().focus.connect([&] (auto &&e) {
        x_txtPassword.select(e.getting);
    });
    x_txtPassword.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_txtQuestion.multi_lines(false).tip_string(u8"Question:");
    x_txtQuestion.events().focus.connect([&] (auto &&e) {
        x_txtQuestion.select(e.getting);
    });
    x_txtQuestion.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnRegister();
    });
    x_txtAnswer.multi_lines(false).tip_string(u8"Answer:");
    x_txtAnswer.events().focus.connect([&] (auto &&e) {
        x_txtAnswer.select(e.getting);
    });
    x_txtAnswer.events().key_press.connect([&] (auto &&e) {
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
}

void FrmRegister::X_OnRegister() {
    wprintf(L"--reg\n");
}

void FrmRegister::X_OnCancel() {
    close();
}
