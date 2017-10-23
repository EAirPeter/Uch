#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmEntry : public nana::form {
public:
    FrmEntry();

private:
    void X_OnLogin();

private:
    nana::place x_pl {*this};
    nana::button x_btnLogin {*this};
    nana::button x_btnQuit {*this};
    nana::button x_btnRegister {*this};
    nana::button x_btnForget {*this};
    nana::textbox x_txtServerHost {*this};
    nana::textbox x_txtServerPort {*this};
    nana::textbox x_txtUsername {*this};
    nana::textbox x_txtPassword {*this};

};
