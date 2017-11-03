#pragma once

#include "Common.hpp"

#include "../UchCommon/Config.hpp"

class UclCfg : public Config {
    friend class Ucl;

private:
    inline UclCfg() : Config(kszFile) {
        Config::SetDefault(kszHost, kszDefHost);
        Config::SetDefault(kszPort, kszDefPort);
        Config::SetDefault(kszLisHost, kszDefLisHost);
    }

public:
    constexpr static decltype(auto) kszFile = L"UchClient.yml";

    constexpr static decltype(auto) kszHost = L"server_host";
    constexpr static decltype(auto) kszPort = L"server_port";
    constexpr static decltype(auto) kszLisHost = L"listen_host";
    constexpr static decltype(auto) kszDefHost = L"localhost";
    constexpr static decltype(auto) kszDefPort = L"54289";
    constexpr static decltype(auto) kszDefLisHost = L"0.0.0.0";

};
