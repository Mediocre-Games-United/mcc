#include "unit_tests.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "packager.hpp"
#include "shell.hpp"
#include "workers.hpp"
#include <format>
#include <functional>

static mcc::compiler::BuildType buildtype = mcc::compiler::BuildType::BUILD_RELEASE;
#ifdef _WIN32
static mcc::compiler::Platform platform = mcc::compiler::Platform::PLATFORM_WINDOWS;
#else
static mcc::compiler::Platform platform = mcc::compiler::Platform::PLATFORM_LINUX;
#endif
static mcc::config::ExportType exptype = mcc::config::ExportType::EXPORT_NONE;

static U8 link_test(mcc::config::ConfigObject *cfg,fpath testpath) {
    fpath build_path = mcc::compiler::get_build_path(cfg,buildtype,platform,exptype,cfg->version);

    if (cfg->model == mcc::config::ConfigModel::SINGLE_EXECUTABLE) {
        string objs = std::format("{}/main.o {}/test.o ",cbu::path_to_utf8(testpath),cbu::path_to_utf8(testpath));
        for (auto &o : cfg->source_files) {
            fpath ofile = mcc::compiler::get_obj_path(o.path,".o");

            objs += std::format("{} ",cbu::path_to_utf8(ofile));
        }

        U8 c = mcc::compiler::link_object(build_path,testpath / mcc::compiler::get_exe_name("run_test",platform),objs,cfg,platform);
        if (c) return c;
    } else {

    }

    return 0;
}
static std::mutex progress_mutex{};
static size_t progress_index = 0;
static size_t progress_max = 0;

static void log_test_progress() {
    progress_mutex.lock();
    progress_index += 1;
    cbu::log_info(std::format("Progress: {}/{} ({:.2f}%)",progress_index,progress_max,F64(progress_index) / F64(progress_max) * 100.0));
    progress_mutex.unlock();
}
static U8 run_test(mcc::config::ConfigObject *cfg) {
    cbu::log_info("Building tests");
    vector<std::function<void()>> work{};
    vector<std::function<void()>> test_work{};

    fpath build_path = mcc::compiler::get_build_path(cfg,buildtype,platform,exptype,cfg->version);
    fpath srcdir = cfg->directory / cfg->src_directory;

    std::atomic<bool> succesful = true;
    bool did_compile = false;
    progress_max = 0;
    progress_index = 0;
    for (auto t : cfg->test_files) {
        string flags = std::format("-DIS_IN_TEST=1 -DCURRENT_TEST_NAME=\"\\\"{}\\\"\"",t.name);

        fpath opath = fpath("tests") / t.name;
        work.push_back([opath,t,build_path,srcdir,&succesful,&did_compile,cfg,flags]() {

            U8 code = mcc::compiler::build_object(srcdir,build_path,{
                .src_path = t.path,
                .obj_path = opath / "test.o",
                .dep_path = opath / "test.d"
            },&did_compile,cfg->version,flags);
            if (code) {
                succesful = false;
                return;
            }

            code = mcc::compiler::build_object(srcdir,build_path,{
                .src_path = cfg->test_main_source.path,
                .obj_path = opath / "main.o",
                .dep_path = opath / "main.d"
            },&did_compile,cfg->version,flags);
            if (code) {
                succesful = false;
                return;
            }

            code = link_test(cfg,opath);
            if (code) {
                succesful = false;
                return;
            }
        });
        test_work.push_back([build_path,opath,&succesful]() {
            string output;

            if (cbu::run_shell_command(build_path,std::format("{}/{}",cbu::path_to_utf8(opath),mcc::compiler::get_exe_name("run_test",platform)),NULL)) {
                succesful = false;
                return;
            }

            log_test_progress();
        });
        progress_max += 1;
    }

    if (work.empty()) {
        cbu::log_warn("Program has no tests!");

        return 0;
    }
    cbu::run_tasks_sync(work);

    if (!succesful) {
        cbu::log_error(false,"Failed to build tests");
        return 1;
    }
    cbu::log_success("Tests built succesfully, starting tests...");

    succesful = true;
    cbu::run_tasks_sync(test_work);

    if (!succesful) {
        cbu::log_error(false,"Tests failed!");
        return 1;
    }
    cbu::log_success("All tests succesful!");

    return 0;
}

U8 mcc::test::run_all_tests(mcc::config::ConfigObject *cfg) {
    U8 c;
    cbu::log_info("Building program...");
    c = mcc::compiler::build_all(cfg,buildtype,platform,cfg->version,exptype);
    if (c) return c;

    cbu::log_info("Creating package...");
    c = mcc::packager::package_all(cfg,buildtype,platform,exptype,cfg->version);
    if (c) return c;

    return run_test(cfg);
}
