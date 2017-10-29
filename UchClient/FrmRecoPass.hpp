#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmRecoPass : public nana::form {
public:
    FrmRecoPass(const nana::form &frmParent, const String &sUser, const String &sQues);

private:
    void X_OnNext();
    void X_OnCancel();

private:
    nana::place x_pl {*this};
    nana::button x_btnNext {*this};
    nana::button x_btnCancel {*this};
    nana::label x_lblUsername {*this};
    nana::label x_lblQuestion {*this};
    nana::textbox x_txtAnswer {*this};
    nana::textbox x_txtPassword {*this};

};
