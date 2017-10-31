#pragma once

#include "Common.hpp"

#include "../UchCommon/EventBus.hpp"
#include "../UchCommon/IoGroup.hpp"

#include "PeerManager.hpp"
#include "UclCfg.hpp"
#include "UclPipl.hpp"

#define USG_NAME Ucl
#define USG_MEMBERS \
    USG_VAL(UclCfg, Cfg, x_vConfig, {}) \
    USG_VAL(IoGroup, Iog, x_vIoGroup, {}) \
    USG_VAL(EventBus, Bus, x_vEventBus, {x_vIoGroup}) \
    USG_VAL(std::unique_ptr<PeerManager>, Pmg, x_upPeerManager, {}) \
    USG_VAL(std::unique_ptr<UclPipl>, Con, x_upPipl, {}) \
    USG_VAL(String, Usr, x_sUsername, {})
#include "../UchCommon/GenSingleton.inl"
