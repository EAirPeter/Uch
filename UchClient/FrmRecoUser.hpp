#pragma once

#include "Common.hpp"


#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmRecoUser : public nana::form {
public:
    FrmRecoUser(const nana::form &frmParent);

private:
    void X_OnNext();
    void X_OnCancel();

private:
    nana::place x_pl {*this};
    nana::button x_btnNext {*this};
    nana::button x_btnCancel {*this};
    nana::textbox x_txtUsername {*this};

};
