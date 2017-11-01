#pragma once

#include "Common.hpp"

#include "Event.hpp"

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/listbox.hpp>

class FrmMain :
    public nana::form,
    public HandlerBase<
        event::EvLog
    >
{
public:
    FrmMain();

public:
    void OnEvent(event::EvLog &e) noexcept override;

private:
    void X_OnList();
    void X_OnDestroy(const nana::arg_unload &e);

    void X_Log(const String &sWhat);

private:
    nana::place x_pl {*this};
    nana::button x_btnList {*this};
    nana::button x_btnStop {*this};
    nana::listbox x_lbxLogs {*this};

};
