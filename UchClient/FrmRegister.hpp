#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmRegister : public nana::form {
public:
    FrmRegister(const nana::form &frmParent);

private:
    void X_OnRegister();
    void X_OnCancel();

private:
    nana::place x_pl {*this};
    nana::button x_btnRegister {*this};
    nana::button x_btnCancel {*this};
    nana::textbox x_txtUsername {*this};
    nana::textbox x_txtPassword {*this};
    nana::textbox x_txtQuestion {*this};
    nana::textbox x_txtAnswer {*this};

};
