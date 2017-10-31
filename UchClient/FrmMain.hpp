#pragma once

#include "Common.hpp"

#include "Event.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/listbox.hpp>

class FrmMain :
    public nana::form,
    public HandlerBase<
        event::EvMessage,
        event::EvListUon,
        event::EvListUff
    >
{
public:
    FrmMain(const nana::form &frmParent);

public:
    void OnEvent(event::EvMessage &e) noexcept override;
    void OnEvent(event::EvListUon &e) noexcept override;
    void OnEvent(event::EvListUff &e) noexcept override;

private:
    void X_OnSend();
    void X_OnFile();
    void X_OnExit();
    void X_OnDestroy(const nana::arg_unload &e);

private:
    nana::place x_pl {*this};
    nana::button x_btnSend {*this};
    nana::button x_btnFile {*this};
    nana::button x_btnExit {*this};
    nana::textbox x_txtMessage {*this};
    nana::listbox x_lbxMessages {*this};
    nana::listbox x_lbxUsers {*this};

};
