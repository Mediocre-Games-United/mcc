#pragma once

#include "config/config_file.hpp"

namespace mcc::compiler {
    enum class BuildType {
        RELEASE =   0,
        BETA =      1,
        DEBUG =     2,
        EDITOR =    3
    };
    enum class Platform {
        WINDOWS =   0,
        LINUX =     1
    };
    std::unordered_map<>

    void build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt);
};
