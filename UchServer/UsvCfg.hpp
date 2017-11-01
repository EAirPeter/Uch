#pragma once

#include "Common.hpp"

#include "../UchCommon/Config.hpp"

class UsvCfg : public Config {
    friend class Usv;

private:
    inline UsvCfg() : Config(kszFile) {
        Config::SetDefault(kszHost, kszDefHost);
        Config::SetDefault(kszPort, kszDefPort);
    }

public:
    constexpr static decltype(auto) kszFile = L"UchServer.yml";

    constexpr static decltype(auto) kszHost = L"listen_host";
    constexpr static decltype(auto) kszPort = L"listen_port";
    constexpr static decltype(auto) kszDefHost = L"0.0.0.0";
    constexpr static decltype(auto) kszDefPort = L"54289";

};
