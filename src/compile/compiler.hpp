#pragma once

#include "config/config_file.hpp"

namespace mcc::compiler {
    enum class BuildType {
        BUILD_RELEASE =   0,
        BUILD_BETA =      1,
        BUILD_DEBUG =     2,
        BUILD_EDITOR =    3
    };
    enum class Platform {
        PLATFORM_WINDOWS =   0,
        PLATFORM_LINUX =     1
    };

    extern const char *build_type_names[sizeof(BuildType)];
    extern const char *platform_names[sizeof(Platform)];
    uint8_t build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt);
};
