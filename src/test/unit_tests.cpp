#include "unit_tests.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "packager.hpp"
#include "workers.hpp"
#include <functional>

static mcc::compiler::BuildType buildtype = mcc::compiler::BuildType::BUILD_EDITOR;
#ifdef _WIN32
static mcc::compiler::Platform platform = mcc::compiler::Platform::PLATFORM_WINDOWS;
#else
static mcc::compiler::Platform platform = mcc::compiler::Platform::PLATFORM_LINUX;
#endif
static mcc::config::ExportType exptype = mcc::config::ExportType::EXPORT_NONE;


static U8 run_test(mcc::config::ConfigObject *cfg) {
    cbu::log_info("Building tests");
    vector<std::function<void()>> work{};

    fpath build_path = mcc::compiler::get_build_path(cfg,buildtype,platform,exptype,cfg->version);
    fpath srcdir = cfg->directory / cfg->src_directory;

    std::atomic<bool> succesful = true;
    bool did_compile = false;
    for (auto t : cfg->test_files) {
        work.push_back([t,build_path,srcdir]() {
            U8 code = mcc::compiler::build_object(srcdir,build_path,{
                .src_path = t.path,
                .obj_path = opath + ".o",
                .dep_path = opath + ".d",
            })
        });
    }

    if (work.empty()) {
        cbu::log_warn("Program has no tests!");

        return 0;
    }
    cbu::run_tasks_sync(work);

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
