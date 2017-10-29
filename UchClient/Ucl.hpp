#pragma once

#include "Common.hpp"

#include "../UchCommon/EventBus.hpp"
#include "../UchCommon/IoGroup.hpp"

#include "UclCfg.hpp"
#include "UclHandler.hpp"

#define USG_NAME Ucl
#define USG_MEMBERS \
    USG_VAL(UclCfg, Cfg, x_vConfig, {}) \
    USG_VAL(IoGroup, Iog, x_vIoGroup, {}) \
    USG_VAL(EventBus, Bus, x_vEventBus, {x_vIoGroup}) \
    USG_VAL(UclHandler, Cmh, x_vCommonHandler, {}) \
    USG_VAL(String, Usr, x_sUsername, {})
#include "../UchCommon/GenSingleton.inl"
