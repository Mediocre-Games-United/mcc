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
static string INCLUDES = "-Iinclude ";
static string DEFINES = "";

static const string DEBUG_FLAGS = "-g -O0 -fno-omit-frame-pointer ";
static const string SUPER_DEBUG_FLAGS = "-fsanitize=address,undefined ";
static const string RELEASE_FLAGS = "-O3 -DNDEBUG ";

static uint8_t build_object(fpath src_path,fpath build_path,mcc::config::SourceCompileTarget tgt,string flags = "") {
    fpath abs_src_path = src_path / tgt.src_path;
    fpath abs_obj_path = build_path / tgt.obj_path;
    if (std::filesystem::exists(abs_obj_path)) {
        auto stamp = std::filesystem::last_write_time(abs_obj_path);
        if (stamp > std::filesystem::last_write_time(abs_src_path)) {
            cbu::log_debug(std::format("{} source is older than object!",cbu::path_to_utf8(tgt.obj_path)));
            return 0;
        }
    }

    std::filesystem::create_directories(abs_obj_path.parent_path());

    string output;
    string cmd = std::format("{} {} {} {} {} -c {} -o {}",
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
    if (cfg->model == mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED) {
        cbu::log_verbose("Build: building subprojects first!");

        uint8_t code;
        for (auto s : cfg->sub_projects) {
            auto code = build_all(s,type,pt);
            if (code) {
                cbu::log_error(false,"Building subproject failed");
                return -1;
            }
        }
    }

    string inc = "";
    get_includes_recurse(inc,cfg->directory);
    INCLUDES = std::format("-I{} {}",cbu::path_to_utf8(cfg->directory / "include"),inc);

    vector<std::optional<cbu::WorkObject>> work;
    fpath build_path = cfg->directory / "build" / std::format("{}{}",platform_names[int(pt)],build_type_names[int(type)]);
    string flags = "";
    if (cfg->model == mcc::config::ConfigModel::SINGLE_SHARED) {
        flags = "-fPIC ";
    }
    switch (type) {
        case BuildType::BUILD_BETA: {
            flags += "-DBETA=1 ";

            break;
        }
        case BuildType::BUILD_DEBUG: {
            flags += "-DDEBUG=1 ";
            flags += DEBUG_FLAGS;
            flags += SUPER_DEBUG_FLAGS;

            break;
        }
        case BuildType::BUILD_EDITOR: {
            flags += "-DDEBUG=1 -DEDITOR=1 ";
            flags += DEBUG_FLAGS;
            flags += SUPER_DEBUG_FLAGS;

            break;
        }
        case BuildType::BUILD_RELEASE: {
            flags += "-DRELEASE=1 ";
            flags += RELEASE_FLAGS;

            break;
        }
        default: break;
    }

    for (auto s : cfg->external_objects) {
        flags += std::format("-I{} ",cbu::path_to_utf8(s->include_path));
    }

    fpath src_path = cfg->directory / cfg->src_directory;

    string link_obj_files = "";
    std::atomic<bool> succesful = true;
    auto srcs = cfg->source_files;
    if (cfg->model == mcc::config::ConfigModel::SINGLE_EXECUTABLE) {
        srcs.push_back(cfg->main_source);
    }

    for (auto &s : srcs) {
        if (!s->enabled) continue;

        const fpath opath = s->path.parent_path();
        string name = s->path.stem();
        string ofile = opath / (name + ".o");
        link_obj_files += std::format(" {}",ofile);

        mcc::config::SourceCompileTarget *tgt = new mcc::config::SourceCompileTarget{
            .src_path = s->path,
            .obj_path = ofile,
            .dep_path = opath / (name + ".d")
        };
        work.push_back(cbu::WorkObject{
            .call = [tgt,&succesful,&src_path,&build_path,&flags]() {
                uint8_t code = build_object(src_path,build_path,*tgt,flags);
                delete tgt;

                log_progress();
                if (code) succesful = false;
            }
        });
    }
    if (work.empty()) {
        cbu::log_info("Everything is up to date or no source files exist!");
        return 0;
    }

    progress_index = 0;
    progress_max = work.size();

    cbu::run_tasks_sync(work);
    if (!succesful) {
        cbu::log_error(false,"Compilation failed!");

        return -1;
    }
    cbu::log_success("Compilation succesful!");

    string linker_output = "";
    string linker_cmd;
    if (cfg->model == mcc::config::ConfigModel::SINGLE_SHARED) {
        linker_cmd = std::format("{} -shared {} -o {} {}",CXX,link_obj_files,
                                 cbu::path_to_utf8(
                                     build_path / std::format("launcher{}",platform_exe[int(pt)])
                                 ),LINKER_FLAGS);
    } else {
        linker_cmd = std::format("{} {} -o {} {}",CXX,link_obj_files,
                                 cbu::path_to_utf8(
                                     build_path / std::format("launcher{}",platform_exe[int(pt)])
                                 ),LINKER_FLAGS);
    }
    uint8_t linker_res = cbu::run_shell_command(build_path,linker_cmd,&linker_output);
    if (linker_res) {
        cbu::log_warn(std::format("Linker returned {} with output {}",linker_res,linker_output));
        return -1;
    }
    cbu::log_success("Linking succesful!");

    return 0;
}
