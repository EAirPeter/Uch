#pragma once

#include "Common.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>

class FrmRecoUser :
    public nana::form,
    public HandlerBase<
        protocol::EvsRecoUserRes
    >
{
public:
    FrmRecoUser(const nana::form &frmParent);

public:
    void OnEvent(protocol::EvsRecoUserRes &e) noexcept override;

private:
    void X_OnNext();
    void X_OnDestroy(const nana::arg_unload &e);

private:
    nana::place x_pl {*this};
    nana::button x_btnNext {*this};
    nana::button x_btnCancel {*this};
    nana::textbox x_txtUsername {*this};

};
