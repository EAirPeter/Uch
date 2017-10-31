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
    caption(String {L"Uch - Client ["} + Ucl::Usr() + L"]");
    events().unload(std::bind(&FrmMain::X_OnDestroy, this, std::placeholders::_1));
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
    x_txtMessage.multi_lines(false).tip_string(u8"Message:");
    x_txtMessage.events().focus.connect([&] (auto &&e) {
        x_txtMessage.select(e.getting);
    });
    x_txtMessage.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnSend();
    });
    x_lbxUsers.enable_single(true, false);
    x_lbxUsers.append_header(L"Name", 190);
    x_lbxUsers.append({L"Online", L"Offline"});
    x_lbxMessages.append_header(L"From", 190);
    x_lbxMessages.append_header(L"Message", 320);
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
    Ucl::Bus().Register(*this);
}

void FrmMain::OnEvent(event::EvMessage &e) noexcept {
    x_lbxMessages.at(0).append(e.vMsg);
}

void FrmMain::OnEvent(event::EvListUon &e) noexcept {
    x_lbxUsers.at(1).model<std::recursive_mutex>(
        e.vecUon,
        [] (auto &c) {
            return AsWideString(c.front().text);
        },
        [] (auto &s) {
            return std::vector<listbox::cell> {AsUtf8String(s)};
        }
    );
}

void FrmMain::OnEvent(event::EvListUff &e) noexcept {
    x_lbxUsers.at(2).model<std::recursive_mutex>(
        e.vecUff,
        [] (auto &c) {
            return AsWideString(c.front().text);
        },
        [] (auto &s) {
            return std::vector<listbox::cell> {AsUtf8String(s)};
        }
    );
}

void FrmMain::X_OnSend() {
    wprintf(L"!send\n");
}

void FrmMain::X_OnFile() {
    wprintf(L"!file\n");
    filebox fb {*this, true};
    if (!fb())
        return;
    auto sPath = AsWideString(fb.file());
    wprintf((sPath + L"\n").c_str());
    form_loader<FrmFileSend> {}(*this, L"mmmmmp", sPath).show();
}

void FrmMain::X_OnExit() {
    wprintf(L"!exit\n");
    auto vec = x_lbxUsers.selected();
    wprintf(L"%u\n", (unsigned) vec.size());
    for (auto &p : vec)
        wprintf(L"%u %u\n", (unsigned) p.cat, (unsigned) p.item);
    this->close();
}

void FrmMain::X_OnDestroy(const nana::arg_unload &e) {
    msgbox mbx {e.window_handle, u8"Uch - Exit", msgbox::yes_no};
    mbx.icon(msgbox::icon_question);
    mbx << L"Are you sure to exit Uch?";
    if (mbx() != mbx.pick_yes)
        e.cancel = true;
    else
        Ucl::Bus().Unregister(*this);
}
