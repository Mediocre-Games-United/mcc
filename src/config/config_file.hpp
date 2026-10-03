#pragma once
#include "base_types.hpp"
#include "logger.hpp"
#include "stringmath.hpp"
#include <cstdint>
#include <format>
#include <queue>
#include <vector>
#include <optional>

#define FNAME "project.mcc"
namespace mcc::config {

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
        string link_name; // name linked with -l[name]

        inline operator bool() const {
            return !name.empty();
        }
    };
    struct ExternalBinary { // package to be downloaded with curl
        string download_url; // url to download from, if empty is blank

        string tld = "./"; // when extracting the package, skip through possible padding, [tld]/include will be included with -I
        string install_cmd = ""; // if non empty, is executed in the tld
        string bin_name; // name for the .dll / .so file so [name].dll/[name].so, will be linked from either [tld]/lib or [tld]/bin as well as lib[name].dll.a for windows

        inline fpath include_path(fpath apath) {
            return apath / tld / "include";
        }
        inline operator bool() const {
            return !download_url.empty();
        }
    };
    struct ExternalObject {
        string name;

        ExternalPackage linux_package{}; // package name from package manager
        ExternalBinary linux_ext_binary{}; // used if no package exists

        ExternalBinary win_ext_binary{}; // used for windows, if empty is skipped (such as a windows built-in)
    };


    enum class ConfigModel : uint8_t {
        SINGLE_EXECUTABLE =      0, // compiles all files to a single executable file and ignores all subprojects. useful for simple or small projects without many dependencies
        SINGLE_SHARED =          1, // compiles all files to a single shared object.
        EXECUTABLES_WITH_SHARED = 2 // compiles subprojects into shared objects and links to them in the executables generated from mains
    };
    enum class ExportType : uint8_t {
        EXPORT_DEFAULT, // Builds and does nothing else on top of it
        EXPORT_ITCHIO, // Builds with some itchio definitions and files and pushes to itchio
        // EXPORT_INSTALLER, // builds and wraps the program in a simple installer (not implemented yet)
        // EXPORT_PORTABLE, // build into a portable zip/tar.gz file (not implemented yet)

        EXPORT_NONE
    };

    struct ConfigObject { // a template object that can be used to generate compile targets, should not have any functionality, only an object describing the project
        ~ConfigObject() {
            cbu::log_verbose(std::format("ConfigObject destroyed at page {}",(void*) this));

            for (auto &s : source_files) {
                delete s;
            }
            if (main_source) delete main_source;
        }
        ConfigObject() {
            cbu::log_verbose(std::format("ConfigObject created at page {}",(void*) this));
        }
        bool is_temp = false;

        string name;
        fpath directory;
        fpath src_directory;
        bool has_parent_directory = false;
        fpath parent_directory = "";
        bool autodetect_source = true;
        ConfigModel model;
        vector<ExportType> export_types{};
        vector<ConfigObject*> sub_projects{};
        vector<SourceFileObject*> source_files{}; // should not include main_source
        vector<ExternalObject*> external_objects{}; // libraries that are linked and included when compiling
        SourceFileObject *main_source = NULL; // used as main for mode 2, otherwise ignored. optionally compiled multiple times

        // other executables only used by mode 2
        bool generate_launcher_wrapper = false; // if true, compile main_source into [main]_app.o -> app(.exe) and [main]_launcher.o -> launcher(.exe) and make the launcher executable a wrapper that handles the app executable
        bool generate_crash_handler = false; // if launcher is set, also generate a crash handler from [main]_crash_handler.o -> crash_handler(.exe)

        string itchio_username;
        string itchio_project;
    };
    struct ExternalWrapper {
        ExternalObject *obj;
        ConfigObject *cfg;
    };

    void init();
    void background();
    bool valid();

    uint8_t reload();
    uint8_t cmd();

    bool link_config(ConfigObject *obj);
    bool link_config_by_file(fpath path);
    bool set_file_current(fpath path);
    bool set_name_current(string name);

    void update_config(ConfigObject *obj);
    ConfigObject *find_config_by_name(string name);
    ConfigObject *find_config_by_path(fpath path);


    struct ConfigContainerDataBlock {
        string fpath;
        ConfigObject *obj = NULL;
    };
    struct ConfigContainer {
        ~ConfigContainer() { for (auto &s : loaded_configs) delete s; }
        vector<ConfigObject*> loaded_configs;
        std::queue<ConfigContainerDataBlock*> pending_datablocks;
    };
    extern ConfigContainer *current_config;
}
