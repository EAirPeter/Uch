#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/progress.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmFileSend : public nana::form {
public:
    FrmFileSend(const nana::form &frmParent, const String &sWhom, const String &sPath);

private:
    void X_OnCancel();

private:
    nana::place x_pl {*this};
    nana::button x_btnCancel {*this};
    nana::label x_lblName {*this};
    nana::label x_lblState {*this};
    nana::label x_lblProg {*this};
    nana::progress x_pgbProg {*this};

};
