#pragma once

#include "config/config_file.hpp"
#include <format>

namespace mcc::compiler {
    enum class BuildType {
        BUILD_RELEASE,
        BUILD_BETA,
        BUILD_DEBUG,
        BUILD_EDITOR,

        BUILD_COUNT
    };
    enum class Platform {
        PLATFORM_WINDOWS,
        PLATFORM_LINUX,

        PLATFORM_COUNT
    };

    extern const char *build_type_names[size_t(BuildType::BUILD_COUNT)];
    extern const char *platform_names[size_t(Platform::PLATFORM_COUNT)];
    extern const char *platform_exe[size_t(Platform::PLATFORM_COUNT)];
    extern const char *platform_shared[size_t(Platform::PLATFORM_COUNT)];
    uint8_t build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt);

    inline fpath get_build_path(mcc::config::ConfigObject *cfg,BuildType type,Platform pt) {
        return cfg->directory / "build" / std::format("{}{}",platform_names[int(pt)],build_type_names[int(type)]);
    }
};
