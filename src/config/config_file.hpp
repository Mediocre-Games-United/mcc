#pragma once
#include "base_types.hpp"
#include "stringmath.hpp"
#include <cstdint>
#include <vector>

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
    struct ExternalObject {
        fpath include_path; // added in -I[path] to compiler
        string link_name = ""; // added in -l[name] to linker if non-empty
    };

    enum class ConfigModel : uint8_t {
        SINGLE_EXECUTABLE =      0, // compiles all files to a single executable file and ignores all subprojects. useful for simple or small projects without many dependencies
        SINGLE_SHARED =          1, // compiles all files to a single shared object.
        EXECUTABLES_WITH_SHARED = 2 // compiles subprojects into shared objects and links to them in the executables generated from mains
    };
    struct ConfigObject { // a template object that can be used to generate compile targets, should not have any functionality, only an object describing the project
        ~ConfigObject() { for (auto &s : source_files) delete s; if (main_source) delete main_source; }
        string name;
        fpath directory;
        fpath src_directory;
        bool has_parent_directory = false;
        fpath parent_directory;
        bool autodetect_source = true;
        ConfigModel model;
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
}
