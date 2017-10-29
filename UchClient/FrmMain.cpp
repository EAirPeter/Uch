#include "Common.hpp"

#include "FrmFileSend.hpp"
#include "FrmMain.hpp"
#include "Ucl.hpp"

#include <nana/gui/filebox.hpp>

using namespace nana;

FrmMain::FrmMain(const nana::form &frmParent) :
    form(frmParent, {768, 480}, appear::decorate<
        appear::taskbar, appear::minimize, appear::maximize, appear::sizable
    >())
{
    caption(String {L"Uch-Client ["} + Ucl::Usr() + L"]");
    events().unload([] (auto &&e) {
        msgbox mbx {e.window_handle, u8"Uch-Client", msgbox::yes_no};
        mbx.icon(msgbox::icon_question);
        mbx << L"Are you sure to exit Uch?";
        e.cancel = mbx() != mbx.pick_yes;
    });
    x_btnSend.caption(L"Send");
    x_btnSend.events().click(std::bind(&FrmMain::X_OnSend, this));
    x_btnSend.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnSend();
    });
    x_btnFile.caption(L"File");
    x_btnFile.events().click(std::bind(&FrmMain::X_OnFile, this));
    x_btnFile.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnFile();
    });
    x_btnExit.caption(L"Exit");
    x_btnExit.events().click(std::bind(&FrmMain::X_OnExit, this));
    x_btnExit.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnExit();
    });
    x_txtMessage.multi_lines(false).enabled(false);
    x_txtMessage.events().focus.connect([&] (auto &&e) {
        x_txtMessage.select(e.getting);
    });
    x_txtMessage.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnSend();
    });
    x_pl.div(
        "margin=[14,16]"
        "   <weight=200 List> <weight=8>"
        "   <vert"
        "       <Msgs> <weight=7>"
        "       <weight=25 Imsg> <weight=7>"
        "       <weight=25 <> <weight=259 gap=8 Btns>>"
        "   >"
    );
    x_pl["List"] << x_lbxUsers;
    x_pl["Msgs"] << x_lbxMessages;
    x_pl["Imsg"] << x_txtMessage;
    x_pl["Btns"] << x_btnExit << x_btnFile << x_btnSend;
    x_pl.collocate();
}

void FrmMain::X_OnSend() {
    wprintf(L"!send\n");
}

void FrmMain::X_OnFile() {
    wprintf(L"!file\n");
    filebox fb {*this, true};
    if (fb()) {
        auto sPath = AsWideString(fb.file());
        wprintf((sPath + L"\n").c_str());
        form_loader<FrmFileSend> {}(*this, L"mmmmmp", sPath).show();
    }
}

void FrmMain::X_OnExit() {
    wprintf(L"!exit\n");
    this->close();
}
