#include "Common.hpp"

#include "Event.hpp"
#include "OnFileReq.hpp"
#include "Ucl.hpp"
#include "UccPipl.hpp"

#include <nana/gui.hpp>
#include <nana/gui/filebox.hpp>

OnFileReq::OnFileReq(UccPipl *pPipl, const protocol::EvpFileReq &e) :
    x_pPipl(pPipl), x_eReq(e)
{
    Ucl::Bus().Register(*this);
}

OnFileReq::~OnFileReq() {
    Ucl::Bus().Unregister(*this);
}

void OnFileReq::OnEvent(protocol::EvpFileCancel &e) noexcept {
    if (e.uId == x_eReq.uId)
        x_atmbCanceled.store(true);
}

void OnFileReq::X_Run() {
    using namespace nana;
    msgbox mbx {nullptr, u8"Uch - Receive file", msgbox::yes_no};
    mbx.icon(msgbox::icon_question);
    mbx << L"[" << x_pPipl->GetUser() << L"] wants to send a file to you.\n";
    mbx << L"Accept " << x_eReq.sName << L" [";
    mbx << FormatSize(x_eReq.uSize, L"GiB", L"MiB", L"KiB", L"B") << L"]?";
    if (mbx() != msgbox::pick_yes) {
        x_pPipl->PostPacket(protocol::EvpFileRes {
            x_eReq.uId, false, 0
        });
        delete this;
        return;
    }
    filebox fbx {nullptr, false};
    fbx.init_path(AsUtf8String(x_eReq.sName));
    if (!fbx()) {
        x_pPipl->PostPacket(protocol::EvpFileRes {
            x_eReq.uId, false, 0
        });
        delete this;
        return;
    }
    auto sPath = AsWideString(fbx.file());
    if (x_atmbCanceled.load()) {
        msgbox mbx {nullptr, u8"Uch - Receive file", msgbox::ok};
        mbx.icon(msgbox::icon_information);
        mbx << L"Transmition canceled";
        mbx();
        delete this;
        return;
    }
    Ucl::Bus().PostEvent(event::EvFileRecv {
        x_pPipl, x_eReq.uId, sPath, x_eReq.uSize, x_eReq.uPort
    });
}
