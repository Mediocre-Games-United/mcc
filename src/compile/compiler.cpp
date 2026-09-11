#include "compiler.hpp"
#include "base_types.hpp"
#include "config/config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "shell/shell.hpp"
#include "workers.hpp"
#include <filesystem>
#include <format>
#include <mutex>


const char *mcc::compiler::build_type_names[sizeof(mcc::compiler::BuildType)] = {
    "RELEASE","BETA","DEBUG","EDITOR"
};
const char *mcc::compiler::platform_names[sizeof(mcc::compiler::Platform)] = {
    "WIN","LINUX"
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

static string CXX = "usr/bin/g++";
static string CXX_FLAGS = "";
static string INCLUDES = "-Iinclude";
static string DEFINES = "";

static void build_object(fpath src_path,fpath build_path,mcc::config::SourceCompileTarget tgt,string flags = "") {
    fpath abs_src_path = src_path / tgt.src_path;
    fpath abs_obj_path = build_path / tgt.obj_path;
    if (std::filesystem::exists(abs_obj_path)) {
        auto stamp = std::filesystem::last_write_time(abs_obj_path);
        if (stamp > std::filesystem::last_write_time(abs_src_path)) cbu::log_debug(std::format("{} source is older than object!",cbu::path_to_utf8(tgt.obj_path)));
    }

    string output;
    string cmd = std::format("{} {} {} {} {} -s {} -o {}",
                             CXX,CXX_FLAGS,INCLUDES,DEFINES,flags,
                             cbu::path_to_utf8(abs_src_path),cbu::path_to_utf8(abs_obj_path));
    auto code = mcc::shell::run_shell_command(build_path,cmd,&output);

    cbu::log_debug(std::format("Got code {} for {}",code,cbu::path_to_utf8(tgt.obj_path)));
    if (code) {
        cbu::log_warn(output);
    }
}

uint8_t mcc::compiler::build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt) {
    vector<std::optional<cbu::WorkObject>> work;
    fpath build_path = cfg->directory / "build" / std::format("{}{}",platform_names[int(pt)],build_type_names[int(type)]);
    string flags = "";
    fpath src_path = cfg->directory / cfg->src_directory;

    for (auto &s : cfg->source_files) {
        if (!s->enabled) continue;

        const fpath opath = s->path.parent_path();
        string name = s->path.stem();
        mcc::config::SourceCompileTarget *tgt = new mcc::config::SourceCompileTarget{
            .src_path = s->path,
            .obj_path = opath / (name + ".o"),
            .dep_path = opath / (name + ".d")
        };
        work.push_back(cbu::WorkObject{
            .call = [tgt,&src_path,&build_path,&flags]() {
                cbu::log_verbose("Doing work...");

                build_object(src_path,build_path,*tgt,flags);
                delete tgt;

                cbu::log_verbose("Work done!");
                log_progress();
            }
        });
    }
    if (work.empty()) {
        cbu::log_info("Everything is up to date or no source files exist!");
        return 0;
    }

    progress_index = 0;
    progress_max = work.size();

    cbu::log_info("Starting work...");
    cbu::run_tasks_sync(work);
    return 0;
}
