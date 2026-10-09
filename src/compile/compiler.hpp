#pragma once

#include "config_file.hpp"
#include "version.hpp"
#include <cassert>
#include <filesystem>
#include <format>

namespace mcc::compiler {
    enum class BuildType {
        BUILD_RELEASE,
        BUILD_BETA,
        BUILD_DEBUG,
        BUILD_EDITOR,

        BUILD_NONE
    };

    enum class Platform {
        PLATFORM_WINDOWS,
        PLATFORM_LINUX,

        PLATFORM_NONE
    };

    extern const char *build_type_names[size_t(BuildType::BUILD_NONE)];
    extern const char *export_type_names[size_t(config::ExportType::EXPORT_NONE)];
    extern const char *platform_names[size_t(Platform::PLATFORM_NONE)];
    extern const char *platform_exe[size_t(Platform::PLATFORM_NONE)];
    extern const char *platform_shared[size_t(Platform::PLATFORM_NONE)];
    U8 build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::version::Version version,mcc::config::ExportType exp);

    inline fpath get_temp_path(mcc::config::ConfigObject *cfg) {
        fpath p = cfg->directory / ".mcc/tmp";
        std::filesystem::create_directories(p);

        return p;
    }
    inline fpath get_external_binary_path(mcc::config::ConfigObject *cfg,string nm,Platform pt) {
        assert(pt != Platform::PLATFORM_NONE);
        fpath p = cfg->directory / ".mcc/install" / platform_names[int(pt)] / nm;
        std::filesystem::create_directories(p);

        return p;
    }
    inline fpath get_export_path(mcc::config::ConfigObject *cfg,mcc::config::ExportType exp,BuildType type,Platform pt,mcc::version::Version ver) {
        assert(type != BuildType::BUILD_NONE);
        assert(pt != Platform::PLATFORM_NONE);

        return cfg->directory / std::format(".mcc/{}/export",mcc::version::get_version_string(ver,"_")) / std::format("{}/{}{}",export_type_names[int(exp)],platform_names[int(pt)],build_type_names[int(type)]);
    }
    inline fpath get_build_path(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::config::ExportType exp,mcc::version::Version ver) {
        assert(type != BuildType::BUILD_NONE);
        assert(pt != Platform::PLATFORM_NONE);

        if (exp != mcc::config::ExportType::EXPORT_NONE) return get_export_path(cfg,exp,type,pt,ver) / "build";
        return cfg->directory / std::format(".mcc/{}/build",mcc::version::get_version_string(ver,"_")) / std::format("{}{}",platform_names[int(pt)],build_type_names[int(type)]);
    }
};
