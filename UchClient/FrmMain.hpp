#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/listbox.hpp>

class FrmMain : public nana::form {
public:
    FrmMain(const nana::form &frmParent);

private:
    void X_OnSend();
    void X_OnFile();
    void X_OnExit();

private:
    nana::place x_pl {*this};
    nana::button x_btnSend {*this};
    nana::button x_btnFile {*this};
    nana::button x_btnExit {*this};
    nana::textbox x_txtMessage {*this};
    nana::listbox x_lbxMessages {*this};
    nana::listbox x_lbxUsers {*this};

};
