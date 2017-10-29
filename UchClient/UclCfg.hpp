#pragma once

#include "Common.hpp"

#include "../UchCommon/Config.hpp"

class UclCfg : public Config {
    friend class Ucl;

private:
    inline UclCfg() : Config(kszFile) {
        Config::SetDefault(kszHost, kszDefHost);
        Config::SetDefault(kszPort, kszDefPort);
    }

public:
    constexpr static decltype(auto) kszFile = L"UchClient.yml";

    constexpr static decltype(auto) kszHost = L"server_host";
    constexpr static decltype(auto) kszPort = L"server_port";
    constexpr static decltype(auto) kszDefHost = L"localhost";
    constexpr static decltype(auto) kszDefPort = L"54289";

};
