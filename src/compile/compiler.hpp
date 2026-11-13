#pragma once

#include "config_file.hpp"
#include "file.hpp"
#include "version.hpp"
#include <cassert>
#include <filesystem>
#include <format>

namespace mcc::compiler {
    extern const BuildTypeStruct build_type_release;
    extern const BuildTypeStruct build_type_beta;
    extern const BuildTypeStruct build_type_editor;
    extern const BuildTypeStruct build_type_debug;
    extern const BuildTypeStruct build_type_stable;

    struct PlatformLinux : PlatformStruct {
        PlatformLinux() : PlatformStruct(1,"linux64","g++") {}

        string get_shared_name(string libname) const override {
            return std::format("lib{}.so",libname);
        }
        string get_exe_name(string exename) const override {
            return exename;
        }
    };
    struct PlatformWindows : PlatformStruct {
        PlatformWindows() : PlatformStruct(0,"win64","x86_64-w64-mingw32-g++") {}

        string get_shared_name(string libname) const override {
            return std::format("{}.dll",libname);
        }
        string get_import_name(string libname) const {
            return std::format("lib{}.dll.a",libname);
        }
        string get_exe_name(string exename) const override {
            return std::format("{}.exe",exename);
        }
    };
    struct PlatformAndroid : PlatformStruct {
        PlatformAndroid() : PlatformStruct(2,"android","android-ndk-cxx") {}

        string get_shared_name(string libname) const override {
            return std::format("lib{}.so",libname);
        }
        string get_exe_name(string exename) const override {
            return exename;
        }
    };
    extern const PlatformLinux platform_linux;
    extern const PlatformWindows platform_windows;
    extern const PlatformAndroid platform_android;

    inline fpath get_obj_path(fpath src_rel,string ext) {
        return fpath("obj") / src_rel.parent_path() / (src_rel.stem() + ext);
    }

    U8 link_object(fpath build_path,fpath output,string link_obj_files,string link_lib_files,mcc::config::ConfigObject *cfg,mcc::compiler::Platform pt,string link_flags_extra = "");
    U8 build_object(fpath src_path,fpath build_path,mcc::config::SourceCompileTarget tgt,bool *did_compile,mcc::version::Version version,string flags = "");
    U8 build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::version::Version &version,mcc::config::ExportType exp,bool overwrite_version = true);

    inline fpath get_temp_path(mcc::config::ConfigObject *cfg) {
        fpath p = cfg->directory / ".mcc/tmp";
        std::filesystem::create_directories(p);

        return p;
    }
    inline fpath get_external_binary_path(mcc::config::ConfigObject *cfg,string nm,Platform pt) {
        fpath p = cfg->directory / ".mcc/install" / pt->name / nm;
        std::filesystem::create_directories(p);

        return p;
    }
    inline string get_mcc_root_path(mcc::config::ConfigObject *cfg,mcc::version::Version ver) {
        return cfg->directory / std::format(".mcc/obj/{}-{}-{}",version::get_version_string(ver,"_"),version::get_build_string(ver),cfg->get_raw_name());
    }
    inline fpath get_export_path(mcc::config::ConfigObject *cfg,mcc::config::ExportType exp,BuildType type,Platform pt,mcc::version::Version ver) {
        return cfg->directory / get_mcc_root_path(cfg,ver) / std::format("export/{}/{}/{}",exp->name,pt->name,type->name);
    }
    inline fpath get_build_path(mcc::config::ConfigObject *cfg,BuildType type,Platform pt,mcc::config::ExportType exp,mcc::version::Version ver) {
        if (exp != NULL) return get_export_path(cfg,exp,type,pt,ver) / "build";
        return cfg->directory / get_mcc_root_path(cfg,ver) / std::format("build/{}/{}",pt->name,type->name);
    }
};
