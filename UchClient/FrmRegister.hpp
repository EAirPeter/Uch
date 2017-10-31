#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmRegister :
    public nana::form,
    public HandlerBase<
        protocol::EvsRegisRes
    >
{
public:
    FrmRegister(const nana::form &frmParent);

public:
    void OnEvent(protocol::EvsRegisRes &e) noexcept override;

private:
    void X_OnRegister();
    void X_OnDestroy(const nana::arg_unload &e);

private:
    nana::place x_pl {*this};
    nana::button x_btnRegister {*this};
    nana::button x_btnCancel {*this};
    nana::textbox x_txtUsername {*this};
    nana::textbox x_txtPassword {*this};
    nana::textbox x_txtQuestion {*this};
    nana::textbox x_txtAnswer {*this};

};
