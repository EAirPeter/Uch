#pragma once

#include "Common.hpp"

#include "../UchCommon/EventBus.hpp"
#include "../UchCommon/IoGroup.hpp"

#include "ClientManager.hpp"
#include "Store.hpp"
#include "UsvCfg.hpp"

#define USG_NAME Usv
#define USG_MEMBERS \
    USG_VAL(UsvCfg, Cfg, x_vConfig, {}) \
    USG_VAL(Store, Sto, x_vStore, {}) \
    USG_VAL(IoGroup, Iog, x_vIoGroup, {}) \
    USG_VAL(EventBus, Bus, x_vEventBus, {x_vIoGroup}) \
    USG_VAL(std::unique_ptr<ClientManager>, Cmg, x_upClientManager, {})
#include "../UchCommon/GenSingleton.inl"
