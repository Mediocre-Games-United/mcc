#include "compiler.hpp"
#include "base_types.hpp"
#include "config/config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "workers.hpp"
#include <filesystem>
#include <format>
#include <mutex>


static vector<fpath> parse_depfile(const string& depfile)
{
    vector<fpath> dependencies;

    // Find the colon separating the target from its dependencies.
    const std::size_t colon = depfile.find(':');
    if (colon == string::npos) {
        return dependencies;
    }

    // Parse only the dependency portion.
    const string input = depfile.substr(colon + 1);

    string token;
    bool escaped = false;

    auto add_token = [&]() {
        if (!token.empty()) {
            dependencies.emplace_back(token);
            token.clear();
        }
    };

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (escaped) {
            // A backslash escapes the following character. This handles
            // escaped spaces, such as "my\\ header.hpp".
            token += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            // Handle Makefile line continuation: backslash followed by '\n'.
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                ++i;
                continue;
            }

            escaped = true;
            continue;
        } else if (c == ':') { // second : signifies to end search
            add_token();
            break;
        }

        if (std::isspace(static_cast<unsigned char>(c))) {
            add_token();
            continue;
        }

        token += c;
    }

    if (escaped) {
        token += '\\';
    }

    add_token();
    return dependencies;
}

const char *mcc::compiler::build_type_names[size_t(BuildType::BUILD_COUNT)] = {
    "RELEASE","BETA","DEBUG","EDITOR"
};
const char *mcc::compiler::platform_names[size_t(Platform::PLATFORM_COUNT)] = {
    "WIN","LINUX"
};
const char *mcc::compiler::platform_shared[size_t(Platform::PLATFORM_COUNT)] {
    ".dll",".so"
};
const char *mcc::compiler::platform_exe[size_t(Platform::PLATFORM_COUNT)] {
    ".exe",""
};

static std::mutex log_mutex;
static size_t progress_index = 0;
static size_t progress_max = 0;
static void log_progress() {
    log_mutex.lock();
    progress_index += 1;
    cbu::log_info(std::format("Progress: {}/{} ({}%)",progress_index,progress_max,double(progress_index) / double(progress_max) * 100));

    log_mutex.unlock();
}

static string CXX = "/usr/bin/g++ ";
static string CXX_FLAGS = "";
static string LINKER_FLAGS = "";
static string LINKER_INCLUDES = "";
static string INCLUDES = "-Iinclude ";
static string DEFINES = "";

static const string DEBUG_FLAGS = "-g -O0 -fno-omit-frame-pointer ";
static const string SUPER_DEBUG_FLAGS = "-fsanitize=address,undefined ";
static const string RELEASE_FLAGS = "-O3 -DNDEBUG ";

static uint8_t build_object(fpath src_path,fpath build_path,mcc::config::SourceCompileTarget tgt,bool *did_compile,string flags = "") {
    fpath abs_src_path = src_path / tgt.src_path;
    fpath abs_obj_path = build_path / tgt.obj_path;
    fpath abs_dep_path = build_path / tgt.dep_path;
    bool should_compile = false;

    auto cstamp = std::filesystem::last_write_time(abs_src_path);

    if (!std::filesystem::exists(abs_obj_path)) {
        cbu::log_verbose("OBJ does not exist! Recompiling...");
        should_compile = true;
    } else {
        const auto obj_stamp = std::filesystem::last_write_time(abs_obj_path);

        // The source file is always a dependency.
        if (std::filesystem::last_write_time(abs_src_path) >= obj_stamp) {
            cbu::log_verbose("Source is newer than obj!");
            should_compile = true;
        }

        // Check included headers and other dependencies.
        if (!should_compile && std::filesystem::exists(abs_dep_path)) {
            const std::string raw = cbu::require_file_path(abs_dep_path);
            const std::vector<fpath> deps = parse_depfile(raw);

            for (const fpath& dep : deps) {
                // Dependency paths in a .d file are usually relative to the
                // directory where the compiler was run.
                fpath abs_dep = dep;

                if (abs_dep.is_relative()) {
                    abs_dep = std::filesystem::current_path() / abs_dep;
                }

                if (!std::filesystem::exists(abs_dep)) {
                    // A deleted or missing dependency should cause recompilation.
                    cbu::log_verbose(std::format("Dependency {} not found!",cbu::path_to_utf8(abs_dep)));
                    should_compile = true;
                    break;
                }

                if (std::filesystem::last_write_time(abs_dep) >= obj_stamp) {
                    cbu::log_verbose("Dependency is newer than object!");
                    should_compile = true;
                    break;
                }
            }
        } else if (!std::filesystem::exists(abs_dep_path)) {
            // Without a dependency file, conservatively rebuild.
            should_compile = true;
        }
    }

    *did_compile = false;
    if (!should_compile) return 0;
    *did_compile = true;

    std::filesystem::create_directories(abs_obj_path.parent_path());

    string output;
    string cmd = std::format("{} -MMD -MP {} {} {} {} -c {} -o {}",
                             CXX,CXX_FLAGS,INCLUDES,DEFINES,flags,
                             cbu::path_to_utf8(abs_src_path),cbu::path_to_utf8(abs_obj_path));
    auto code = cbu::run_shell_command(build_path,cmd,&output);

    cbu::log_debug(std::format("Got code {} for {}",code,cbu::path_to_utf8(tgt.obj_path)));
    if (code) {
        cbu::log_warn(output);
    }

    return code;
}

static void get_includes_recurse(string &output,fpath dir) {
    if (!std::filesystem::is_directory(dir)) return;

    string stem = dir.stem();
    if (stem == ".git" || stem == "build" || stem == "export") return;
    output += std::format(" -I{}",cbu::path_to_utf8(dir));

    for (auto &s : cbu::iterate_dir(dir)) {
        get_includes_recurse(output,s);
    }
}

uint8_t mcc::compiler::build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt) {
    auto external = cfg->external_objects;

    if (cfg->model == mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED) {
        cbu::log_verbose("Build: building subprojects first!");

        uint8_t code;
        for (auto s : cfg->sub_projects) {
            for (auto e : s->external_objects) {
                external.push_back(e);
            }


            auto code = build_all(s,type,pt);
            if (code) {
                cbu::log_error(false,"Building subproject failed");
                return -1;
            }
            cbu::log_success("Subproject built succesfully");
        }
    }

    string inc = "";
    get_includes_recurse(inc,cfg->directory);
    INCLUDES = std::format("-I{} {} ",cbu::path_to_utf8(cfg->directory / "include"),inc);
    if (cfg->has_parent_directory) {
        fpath pdir = cfg->directory / cfg->parent_directory;

        INCLUDES = std::format("{} -I{} -I{}",INCLUDES,
                               cbu::path_to_utf8(pdir / "src"),
                               cbu::path_to_utf8(pdir / "include"));
    }

    vector<std::optional<cbu::WorkObject>> work;
    fpath build_path = get_build_path(cfg,type,pt);
    cbu::log_info(std::format("Build path: {}",cbu::path_to_utf8(build_path)));

    string flags = "";
    if (cfg->model != mcc::config::ConfigModel::SINGLE_EXECUTABLE) {
        flags = "-fPIC ";
    }
    CXX_FLAGS = "";
    switch (type) {
        case BuildType::BUILD_BETA: {
            CXX_FLAGS += "-DBETA=1 ";

            break;
        }
        case BuildType::BUILD_DEBUG: {
            CXX_FLAGS += "-DDEBUG=1 ";
            CXX_FLAGS += DEBUG_FLAGS;
            CXX_FLAGS += SUPER_DEBUG_FLAGS;

            break;
        }
        case BuildType::BUILD_EDITOR: {
            CXX_FLAGS += "-DDEBUG=1 -DEDITOR=1 ";
            CXX_FLAGS += DEBUG_FLAGS;
            CXX_FLAGS += SUPER_DEBUG_FLAGS;

            break;
        }
        case BuildType::BUILD_RELEASE: {
            CXX_FLAGS += "-DRELEASE=1 ";
            CXX_FLAGS += RELEASE_FLAGS;

            break;
        }
        default: break;
    }

    LINKER_INCLUDES = "";
    for (auto s : external) {
        flags += std::format("-I{} ",cbu::path_to_utf8(s->include_path));
        if (s->link_name.empty()) continue;
        LINKER_INCLUDES += std::format("-l{} ",s->link_name);
    }

    fpath src_path = cfg->directory / cfg->src_directory;

    string link_obj_files = "";
    std::atomic<bool> succesful = true;
    std::atomic<bool> any_compiled = false;
    auto srcs = cfg->source_files;
    if (cfg->model == mcc::config::ConfigModel::SINGLE_EXECUTABLE) {
        srcs.push_back(cfg->main_source);
    }

    for (auto &s : srcs) {
        if (!s->enabled) continue;

        const fpath opath = "obj" / s->path.parent_path();
        string name = s->path.stem();
        string ofile = opath / (name + ".o");
        link_obj_files += std::format(" {}",ofile);

        mcc::config::SourceCompileTarget *tgt = new mcc::config::SourceCompileTarget{
            .src_path = s->path,
            .obj_path = ofile,
            .dep_path = opath / (name + ".d")
        };
        work.push_back(cbu::WorkObject{
            .call = [tgt,&succesful,&src_path,&build_path,&flags,&any_compiled]() {
                bool did_compile;
                uint8_t code = build_object(src_path,build_path,*tgt,&did_compile,flags);
                delete tgt;

                log_progress();
                if (code) succesful = false;
                if (did_compile) any_compiled = true;
            }
        });
    }
    fpath link_path;
    if (cfg->model != mcc::config::ConfigModel::SINGLE_EXECUTABLE) {
        link_path = build_path / std::format("{}{}",cfg->name,platform_shared[int(pt)]);
        link_obj_files = std::format("-shared {}",link_obj_files);
    } else {
        link_path = build_path / std::format("launcher{}",platform_exe[int(pt)]);
    }
    cbu::log_info(std::format("Link path: {}",cbu::path_to_utf8(link_path)));

    if (work.empty()) {
        if (cfg->model != mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED) {
            cbu::log_info("Everything is up to date or no source files exist!");
            return 0;
        }
    } else {
        progress_index = 0;
        progress_max = work.size();

        cbu::run_tasks_sync(work);
        if (!succesful) {
            cbu::log_error(false,"Compilation failed!");

            return -1;
        }
        cbu::log_success("Compilation succesful!");
        if (any_compiled) {
            string linker_output = "";
            string linker_cmd = std::format("{} {} {} -o {} {}",CXX,CXX_FLAGS,link_obj_files,
                                            cbu::path_to_utf8(link_path),LINKER_FLAGS);;
            uint8_t linker_res = cbu::run_shell_command(build_path,linker_cmd,&linker_output);
            if (linker_res) {
                cbu::log_warn(std::format("Linker returned {} with output {}",linker_res,linker_output));
                return -1;
            }


        }
    }

    if (cfg->model == mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED) {
        mcc::config::SourceCompileTarget main_tgt = {
            .src_path = cfg->main_source->path,
            .obj_path = "main.o",
            .dep_path = "main.d"
        };
        bool did_compile;
        uint8_t mcode = build_object(src_path,build_path,main_tgt,&did_compile,flags + " -DRUN_APP_MODE=1");
        if (mcode) {
            cbu::log_error(false,"Building main failed");
            return -1;
        }
        link_obj_files = std::format("main.o {} ",cbu::path_to_utf8(link_path));
        for (auto s : cfg->sub_projects) {
            fpath build_path = get_build_path(s,type,pt);
            link_obj_files = std::format("{} {}",link_obj_files,
                                         cbu::path_to_utf8(
                                             build_path / std::format("{}{}",s->name,platform_shared[int(pt)])));
        }

        string linker_output = "";
        string linker_cmd = std::format("{} {} {} -o {} {} {}",CXX,CXX_FLAGS,link_obj_files,
                                 cbu::path_to_utf8(
                                     build_path / std::format("launcher{}",platform_exe[int(pt)])
                                 ),LINKER_INCLUDES,LINKER_FLAGS);

        uint8_t linker_res = cbu::run_shell_command(build_path,linker_cmd,&linker_output);
        if (linker_res) {
            cbu::log_warn(std::format("Linker returned {} with output {}",linker_res,linker_output));
            return -1;
        }
    }

    cbu::log_success("Linking succesful!");

    return 0;
}
