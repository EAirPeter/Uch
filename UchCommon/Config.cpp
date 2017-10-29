#include "Common.hpp"

#include "Config.hpp"
#include "String.hpp"

#pragma warning(push)
#pragma warning(disable: 4127) // if constexpr
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <fstream>

void Config::Load() {
    unordered_map::clear();
    try {
        auto vYaml = YAML::LoadFile(AsUtf8String(x_sPath));
        for (const auto &p : vYaml) {
            auto sKey = AsWideString(p.first.as<std::string>());
            auto sVal = AsWideString(p.second.as<std::string>());
            unordered_map::emplace(std::move(sKey), std::move(sVal));
        }
    }
    catch (YAML::BadFile &) {
    }
    catch (YAML::ParserException &) {
    }
    for (auto &p : x_mapDefaults) {
        auto it = unordered_map::find(p.first);
        if (it == unordered_map::end())
            unordered_map::emplace(p.first, p.second);
    }
}

void Config::Save() {
    YAML::Node vYaml;
    for (auto &p : *static_cast<unordered_map *>(this))
        vYaml[AsUtf8String(p.first)] = AsUtf8String(p.second);
    std::ofstream vStream(AsUtf8String(x_sPath));
    vStream << vYaml;
}
