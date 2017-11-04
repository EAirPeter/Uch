#pragma once

#include "Common.hpp"

#include "../UchCommon/EventBus.hpp"
#include "../UchCommon/IoGroup.hpp"
#include "../UchCommon/String.hpp"

#include "PeerManager.hpp"
#include "UclCfg.hpp"
#include "UclPipl.hpp"

#define USG_NAME Ucl
#define USG_MEMBERS \
    USG_VAL(String, Ucf, x_sUchFile, {}) \
    USG_VAL(UclCfg, Cfg, x_vConfig, {}) \
    USG_VAL(IoGroup, Iog, x_vIoGroup, {TP_CALLBACK_PRIORITY_NORMAL, 1000}) \
    USG_VAL(EventBus, Bus, x_vEventBus, {x_vIoGroup}) \
    USG_VAL(std::unique_ptr<PeerManager>, Pmg, x_upPeerManager, {}) \
    USG_VAL(std::unique_ptr<UclPipl>, Con, x_upPipl, {}) \
    USG_VAL(String, Usr, x_sUsername, {}) \
    USG_VAL(String, Upx, x_sUchPrefix, {})
#include "../UchCommon/GenSingleton.inl"

inline String Title(const String &s) {
    return Ucl::Upx() + s;
}

inline std::string TitleU8(const String &s) {
    return AsUtf8String(Ucl::Upx() + s);
}
