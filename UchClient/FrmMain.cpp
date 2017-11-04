#include "Common.hpp"

#include "FrmFileRecv.hpp"
#include "FrmFileSend.hpp"
#include "FrmMain.hpp"
#include "Ucl.hpp"

#include <nana/gui/filebox.hpp>

using namespace nana;

FrmMain::FrmMain() :
    form(nullptr, {768, 480}, appear::decorate<
        appear::taskbar, appear::minimize, appear::maximize, appear::sizable
    >())
{
    caption(Title(L"Client"));
    events().destroy(std::bind(&FrmMain::X_OnDestroy, this, std::placeholders::_1));
    events().user(std::bind(&FrmMain::X_OnUser, this, std::placeholders::_1));
    events().unload([this] (const arg_unload &e) {
        msgbox mbx {nullptr, TitleU8(L"Exit"), msgbox::yes_no};
        mbx.icon(msgbox::icon_question);
        mbx << L"Are you sure to exit Uch?";
        if (mbx() != mbx.pick_yes)
            e.cancel = true;
    });
    x_btnSend.caption(L"Send");
    x_btnSend.events().click(std::bind(&FrmMain::X_OnSend, this));
    x_btnSend.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnSend();
    });
    x_btnFile.caption(L"Send File");
    x_btnFile.events().click(std::bind(&FrmMain::X_OnFile, this));
    x_btnFile.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnFile();
    });
    x_btnExit.caption(L"Exit");
    x_btnExit.events().click(std::bind(&FrmMain::close, this));
    x_btnExit.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            close();
    });
    x_txtMessage.multi_lines(false).tip_string(u8"Message:");
    x_txtMessage.events().focus([this] (const arg_focus &e) {
        x_txtMessage.select(e.getting);
    });
    x_txtMessage.events().key_press([this] (const arg_keyboard &e) {
        if (e.key == keyboard::enter)
            X_OnSend();
    });
    x_lbxUsers.enable_single(true, false);
    x_lbxUsers.append_header(L"Who", 110);
    x_lbxUsers.append({L"Online", L"Offline"});
    x_lbxMessages.sortable(false);
    x_lbxMessages.append_header(L"When", 80);
    x_lbxMessages.append_header(L"How", 180);
    x_lbxMessages.append_header(L"What", 310);
    x_pl.div(
        "margin=[14,16]"
        "   <weight=120 List> <weight=8>"
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
    constexpr static auto kszFmt = L"(%s) %s => %s";
    x_lbxMessages.at(0).append({
        FormattedTime(),
        FormatString(L"(%s) %s => %s", e.sCat.c_str(), e.sFrom.c_str(), e.sTo.c_str()),
        e.sWhat
    });
    x_lbxMessages.scroll(true);
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

void FrmMain::OnEvent(event::EvFileReq &e) noexcept {
    user(std::make_unique<event::EvFileReq>(e).release());
}

void FrmMain::X_OnSend() {
    auto vec = x_lbxUsers.selected();
    if (vec.empty()) {
        msgbox mbx {nullptr, TitleU8(L"Send message"), msgbox::ok};
        mbx.icon(msgbox::icon_error);
        mbx << L"Please select a recipient";
        mbx();
        return;
    }
    auto &idx = vec.front();
    auto sUser = AsWideString(x_lbxUsers.at(idx.cat).at(idx.item).text(0));
    switch (idx.cat) {
    case 1:
        // Online
        X_AddMessage({kszCatChat, kszSelf, sUser, x_txtMessage.caption_wstring()});
        (*Ucl::Pmg())[sUser].PostPacket(protocol::EvpMessage {
            x_txtMessage.caption_wstring()
        });
        break;
    case 2:
        // Offline
        X_AddMessage({kszCatFmsg, kszSelf, sUser, x_txtMessage.caption_wstring()});
        Ucl::Con()->PostPacket(protocol::EvcMessageTo {
            sUser, x_txtMessage.caption_wstring()
        });
        break;
    default:
        throw ExnIllegalState {};
    }
    x_txtMessage.caption(String {});
}

void FrmMain::X_OnFile() {
    auto vec = x_lbxUsers.selected();
    if (vec.empty()) {
        msgbox mbx {TitleU8(L"Send file")};
        mbx.icon(msgbox::icon_error);
        mbx << L"Please select a recipient";
        mbx();
        return;
    }
    auto &idx = vec.front();
    if (idx.cat != 1) {
        msgbox mbx {TitleU8(L"Send file")};
        mbx.icon(msgbox::icon_error);
        mbx << L"Please select an online user";
        mbx();
        return;
    }
    auto sUser = AsWideString(x_lbxUsers.at(idx.cat).at(idx.item).text(0));
    filebox fbx {nullptr, true};
    if (!fbx())
        return;
    auto sPath = AsWideString(fbx.file());
    UccPipl *pPipl;
    try {
        pPipl = &(*Ucl::Pmg())[sUser];
    }
    catch (std::out_of_range) {
        msgbox mbx {TitleU8(L"Send file")};
        mbx << L"The user if offline";
        mbx();
        return;
    }
    form_loader<FrmFileSend>() (*this, pPipl, sPath).show();
}

void FrmMain::X_OnDestroy(const nana::arg_destroy &e) {
    Ucl::Con()->PostPacket(protocol::EvcExit {Ucl::Usr()});
    Ucl::Con()->Shutdown();
    Ucl::Pmg()->Shutdown();
    Ucl::Bus().Unregister(*this);
}

void FrmMain::X_OnUser(const nana::arg_user &e) {
    std::unique_ptr<event::EvFileReq> up {
        reinterpret_cast<event::EvFileReq *>(e.param)
    };
    try {
        form_loader<FrmFileRecv>() (*this, up->pPipl, up->eReq).show();
    }
    catch (ExnIllegalState) {}
}

void FrmMain::X_AddMessage(event::EvMessage &&e) noexcept {
    OnEvent(e);
}
