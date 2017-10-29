#include "Common.hpp"

#include "FrmFileSend.hpp"

using namespace nana;

FrmFileSend::FrmFileSend(const form &frmParent, const String &sWhom, const String &sPath) :
    form(frmParent, {480, 180}, appear::decorate<appear::taskbar, appear::minimize>())
{
    caption(String {L"Uch-Sending file to ["} + sWhom + L"]");
    x_btnCancel.caption(L"Cancel");
    x_btnCancel.events().click(std::bind(&FrmFileSend::X_OnCancel, this));
    x_btnCancel.events().key_press.connect([&] (auto &&e) {
        if (e.key == keyboard::enter)
            X_OnCancel();
    });
    x_lblName.caption(sPath);
    x_lblState.caption(L"Waiting to be accepted...");
    x_lblProg.text_align(align::right, align_v::center).caption(L"233%");
    x_pl.div(
        "margin=[14,16] vert"
        "   <vfit=448 Name> <weight=7>"
        "   <weight=25 arrange=[40,variable] gap=8 Prog> <weight=7>"
        "   <vfit=448 Stat> <>"
        "   <weight=25 <> <weight=81 Canc>>"
    );
    x_pl["Name"] << x_lblName;
    x_pl["Prog"] << x_lblProg << x_pgbProg;
    x_pl["Stat"] << x_lblState;
    x_pl["Canc"] << x_btnCancel;
    x_pl.collocate();
}

void FrmFileSend::X_OnCancel() {
    wprintf(L"?cancel\n");
}
