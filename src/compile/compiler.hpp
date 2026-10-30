#pragma once

#include "config_file.hpp"
#include "file.hpp"
#include "version.hpp"
#include <cassert>
#include <filesystem>
#include <format>

namespace mcc::compiler {
    extern const char *build_type_names[size_t(BuildType::BUILD_NONE)];
    extern const char *export_type_names[size_t(config::ExportType::EXPORT_NONE)];
    extern const char *platform_names[size_t(Platform::PLATFORM_NONE)];
    extern const char *platform_exe[size_t(Platform::PLATFORM_NONE)];
    extern const char *platform_shared[size_t(Platform::PLATFORM_NONE)];

    U8 build_object(fpath src_path,fpath build_path,mcc::config::SourceCompileTarget tgt,bool *did_compile,mcc::version::Version version,string flags = "");
    U8 build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::version::Version &version,mcc::config::ExportType exp,bool overwrite_version = true);

    inline string get_shared_lib_name(string name,Platform pt) {
        if (pt == Platform::PLATFORM_WINDOWS) return std::format("{}{}",name,platform_shared[int(pt)]);
        return std::format("lib{}{}",name,platform_shared[int(pt)]);
    }
    inline string get_exe_name(string nm,Platform pt) {
        return std::format("{}{}",nm,platform_exe[int(pt)]);
    }
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
    inline string get_mcc_root_path(mcc::config::ConfigObject *cfg,mcc::version::Version ver) {
        return cbu::path_to_utf8(cfg->directory / std::format(".mcc/{}-{}-{}",version::get_version_string(ver,"_"),version::get_build_string(ver),cfg->get_raw_name()));
    }
    inline fpath get_export_path(mcc::config::ConfigObject *cfg,mcc::config::ExportType exp,BuildType type,Platform pt,mcc::version::Version ver) {
        assert(type != BuildType::BUILD_NONE);
        assert(pt != Platform::PLATFORM_NONE);

        return cfg->directory / get_mcc_root_path(cfg,ver) / std::format("export/{}/{}/{}",export_type_names[int(exp)],platform_names[int(pt)],build_type_names[int(type)]);
    }
    inline fpath get_build_path(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::config::ExportType exp,mcc::version::Version ver) {
        assert(type != BuildType::BUILD_NONE);
        assert(pt != Platform::PLATFORM_NONE);

        if (exp != mcc::config::ExportType::EXPORT_NONE) return get_export_path(cfg,exp,type,pt,ver) / "build";
        return cfg->directory / get_mcc_root_path(cfg,ver) / std::format("build/{}/{}",platform_names[int(pt)],build_type_names[int(type)]);
    }
};
