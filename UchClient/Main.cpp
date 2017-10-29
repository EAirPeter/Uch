#pragma once

#include "Common.hpp"

#include "FrmEntry.hpp"
#include "Ucl.hpp"

Ucl Ucl::x_vInstance {};

int wmain() {
    System::GlobalStartup();
    Ucl::Cfg().Load();
    Ucl::Iog().Start();
    FrmEntry frmEntry;
    frmEntry.show();
    nana::exec();
    Ucl::Iog().Shutdown();
    Ucl::Cfg().Save();
    return 0;
}
