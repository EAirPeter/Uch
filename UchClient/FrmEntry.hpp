#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmEntry : public nana::form, public HandlerBase<protocol::EvsLoginRes> {
public:
    FrmEntry();
    
public:
    void OnEvent(protocol::EvsLoginRes &e) noexcept override;

private:
    void X_OnLogin();
    void X_OnRegister();
    void X_OnForget();
    void X_OnDestroy(const nana::arg_unload &e);

private:
    nana::place x_pl {*this};
    nana::button x_btnLogin {*this};
    nana::button x_btnExit {*this};
    nana::button x_btnRegister {*this};
    nana::button x_btnForget {*this};
    nana::textbox x_txtUsername {*this};
    nana::textbox x_txtPassword {*this};

};
