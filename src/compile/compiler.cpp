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

static std::mutex log_mutex;
static size_t progress_index = 0;
static size_t progress_max = 0;
static void log_progress() {
    log_mutex.lock();
    progress_index += 1;
    cbu::log_info(std::format("Progress: {}/{} ({}%)",progress_index,progress_max,double(progress_index) / double(progress_max) * 100));

    log_mutex.unlock();
}

static string CXX;
static string CXX_FLAGS;
static string INCLUDES;
static string DEFINES;

static void build_object(fpath build_path,mcc::config::SourceCompileTarget tgt,string flags = "") {
    fpath abs_obj_path = build_path / tgt.obj_path;
    if (std::filesystem::exists(abs_obj_path)) {
        auto stamp = std::filesystem::last_write_time(abs_obj_path);
        if (stamp > std::filesystem::last_write_time(tgt.src_path)) return;
    }

    string output;
    auto code = mcc::shell::run_shell_command(build_path,std::format("{} {} {} {} {} -s {} -o {}",
                                                          CXX,CXX_FLAGS,INCLUDES,DEFINES,flags,
                                                          cbu::path_to_utf8(tgt.src_path),
                                                          cbu::path_to_utf8(tgt.obj_path)),
                                              &output);


}

void mcc::compiler::build_all(mcc::config::ConfigObject *cfg,BuildType type,Platform pt) {
    vector<std::optional<cbu::WorkObject>> work;
    fpath build_path = cfg->directory / "build";
    for (auto &s : cfg->source_files) {
        if (!s->enabled) continue;

        mcc::config::SourceCompileTarget tgt;
        work.push_back(cbu::WorkObject{
            .call = [&s,tgt,build_path]() {
                build_object(build_path,tgt);
            }
        });
    }

    cbu::run_tasks_sync(work);
}
