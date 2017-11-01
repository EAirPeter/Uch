#include "Common.hpp"

#include "FrmMain.hpp"
#include "Usv.hpp"

using namespace nana;

FrmMain::FrmMain() :
    form(API::make_center(768, 480), appear::decorate<
        appear::taskbar, appear::minimize, appear::maximize, appear::sizable
    >())
{
    caption(String {L"Uch - Server "} + static_cast<String>(Usv::Cmg()->GetListenSockName()));
    events().unload(std::bind(&FrmMain::X_OnDestroy, this, std::placeholders::_1));
    x_btnList.caption(L"List Users");
    x_btnList.events().click(std::bind(&FrmMain::X_OnList, this));
    x_btnList.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnList();
    });
    x_btnStop.caption(L"Stop");
    x_btnStop.events().click(std::bind(&FrmMain::close, this));
    x_btnStop.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_lbxLogs.sortable(false);
    x_lbxLogs.append_header(L"When", 80);
    x_lbxLogs.append_header(L"What", 520);
    x_pl.div(
        "margin=[14,16]"
        "   <Logs> <weight=8>"
        "   <vert weight=81 gap=7 arrange=[25,repeated] Btns>"
    );
    x_pl["Logs"] << x_lbxLogs;
    x_pl["Btns"] << x_btnList << x_btnStop;
    x_pl.collocate();
    Usv::Bus().Register(*this);
}

void FrmMain::OnEvent(event::EvLog &e) noexcept {
    X_Log(e.sWhat);
}

void FrmMain::X_OnList() {
    auto &&vecUon = Usv::Sto().GetUon();
    auto &&vecUff = Usv::Sto().GetUff();
    X_Log(std::to_wstring(vecUon.size()) + L" users online:");
    for (auto &vUsr : vecUon)
        X_Log(vUsr.sName + L' ' + static_cast<String>(vUsr.vAddr));
    String sList = std::to_wstring(vecUff.size()) + L" users offline: ";
    for (auto &sUsr : vecUff) {
        sList += std::move(sUsr);
        sList += L", ";
    }
    sList.pop_back();
    sList.pop_back();
    X_Log(sList);
}

void FrmMain::X_OnDestroy(const nana::arg_unload &e) {
    msgbox mbx {e.window_handle, u8"Uch - Exit", msgbox::yes_no};
    mbx.icon(msgbox::icon_question);
    mbx << L"Are you sure to stop the Uch server?";
    if (mbx() != mbx.pick_yes) {
        e.cancel = true;
        return;
    }
    Usv::Cmg()->Shutdown();
    Usv::Bus().Unregister(*this);
}

void FrmMain::X_Log(const String &sWhat) {
    x_lbxLogs.at(0).append({FormattedTime(), sWhat});
    x_lbxLogs.scroll(true);
}
