#pragma once
#include "base_types.hpp"
#include "stringmath.hpp"
#include <cstdint>
#include <vector>
#include <optional>

namespace mcc::config {
    static const char *FNAME = "project.mcc";

    struct SourceCompileTarget {
        fpath src_path;
        fpath obj_path;
        fpath dep_path;
    };
    struct SharedCompileTarget {
        string shared_object_name;
        std::vector<SourceCompileTarget> objects;
    };
    struct ExecutableCompileTarget {
        string executable_name;
        std::vector<SourceCompileTarget> objects;
        std::vector<SharedCompileTarget> shared;
    };
    struct SourceFileObject { // used as a guide to generate SourceCompileTarget objects
        fpath path;
        string name;
        bool enabled = true;
    };

    struct ExternalPackage { // package from a package manager distribution
        string name = ""; // if empty, is blank
        string include_path; // absolute path included with -I[path]
        string link_name; // name linked with -l[name]

        inline operator bool() const {
            return !name.empty();
        }
    };
    struct ExternalBinary { // package to be downloaded with curl
        string name = ""; // if empty, is blank
        string download_url; // url to download from

        string include_path; // relative path to the archive to be included with -I[path]
        string bin_name; // name for the .dll / .so file so [name].dll/.so, will be recursively searched as well as lib[name].dll.a for windows

        inline operator bool() const {
            return !name.empty() && !download_url.empty();
        }
    };
    struct ExternalObject {
        string name;

        ExternalPackage linux_package{}; // package name from package manager
        ExternalPackage apt_package{}; // package override for apt
        ExternalPackage pacman_package{}; // package override for pacman
        ExternalBinary linux_ext_binary{}; // used if no package exists

        ExternalBinary win_ext_binary{}; // used for windows, if empty is skipped
    };

    enum class ConfigModel : uint8_t {
        SINGLE_EXECUTABLE =      0, // compiles all files to a single executable file and ignores all subprojects. useful for simple or small projects without many dependencies
        SINGLE_SHARED =          1, // compiles all files to a single shared object.
        EXECUTABLES_WITH_SHARED = 2 // compiles subprojects into shared objects and links to them in the executables generated from mains
    };
    enum class ExportType : uint8_t {
        EXPORT_DEFAULT, // Builds and does nothing else on top of it
        // EXPORT_INSTALLER, // builds and wraps the program in a simple installer (not implemented yet)
        // EXPORT_PORTABLE, // build into a portable zip/tar.gz file (not implemented yet)

        EXPORT_NONE
    };

    struct ConfigObject { // a template object that can be used to generate compile targets, should not have any functionality, only an object describing the project
        ~ConfigObject() {
            for (auto &s : source_files) {
                delete s;
            }
            if (main_source) delete main_source;
        }
        string name;
        fpath directory;
        fpath src_directory;
        bool has_parent_directory = false;
        fpath parent_directory;
        bool autodetect_source = true;
        ConfigModel model;
        std::vector<ExportType> export_types;
        std::vector<ConfigObject*> sub_projects;
        std::vector<SourceFileObject*> source_files; // should not include main_source
        std::vector<ExternalObject*> external_objects; // libraries that are linked and included when compiling
        SourceFileObject *main_source; // used as main for mode 2, otherwise ignored. optionally compiled multiple times

        // other executables only used by mode 2
        bool generate_launcher_wrapper = false; // if true, compile main_source into [main]_app.o -> app(.exe) and [main]_launcher.o -> launcher(.exe) and make the launcher executable a wrapper that handles the app executable
        bool generate_crash_handler = false; // if launcher is set, also generate a crash handler from [main]_crash_handler.o -> crash_handler(.exe)
    };

    void init();
    void background();
    bool valid();

    uint8_t reload();
    uint8_t cmd();

    bool link_file(fpath path);
    bool set_file_current(fpath path);
    bool set_name_current(string name);

    void update_config_src(ConfigObject *obj);
}
