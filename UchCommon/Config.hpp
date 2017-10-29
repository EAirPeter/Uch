#pragma once

#include "Common.hpp"

class Config : public std::unordered_map<String, String> {
public:
    inline Config(const String &sPath) : x_sPath(sPath) {}

    inline Config &SetDefaults(const std::unordered_map<String, String> &mapDefaults) {
        x_mapDefaults = mapDefaults;
        return *this;
    }

    inline Config &SetDefault(const String &sKey, const String &sVal) {
        x_mapDefaults.emplace(sKey, sVal);
        return *this;
    }

    void Load();
    void Save();

private:
    String x_sPath;
    std::unordered_map<String, String> x_mapDefaults;

};
