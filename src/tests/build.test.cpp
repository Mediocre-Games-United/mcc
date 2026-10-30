#include "compiler.hpp"
#include "logger.hpp"
#include "packager.hpp"
#include "state.hpp"
#include "unit_test.hpp"
#include "config_file.hpp"
#include "test_helpers.hpp"
#include <format>

using namespace cbu::test;

void cbu::test::test_main(test_result &res,int argc,char **argv) {
    wrap_bareminimum([&res]() {
        if (!mcc::config::set_name_current("mcc")) {
            res.set_fail("Could not set mcc as the current config");

            return;
        }
        U8 code = 0;
        mcc::state::state_safe([&code,&res]() {
            auto cfg = mcc::state::active_config;
            vector<mcc::compiler::BuildType> build_types = {mcc::compiler::BuildType::BUILD_EDITOR,mcc::compiler::BuildType::BUILD_RELEASE};
            vector<mcc::compiler::Platform> build_platforms = {mcc::compiler::Platform::PLATFORM_LINUX,mcc::compiler::Platform::PLATFORM_WINDOWS};

            for (auto &buildtype : build_types) {
                for (auto &platform : build_platforms) {
                    code = mcc::compiler::build_all(cfg,buildtype,platform,cfg->version,mcc::config::ExportType::EXPORT_NONE,true);
                    if (code) {
                        res.set_fail(std::format("Failed to build mcc {} for {}",mcc::compiler::build_type_names[int(buildtype)],mcc::compiler::platform_names[int(platform)]));

                        return;
                    }

                    code = mcc::packager::package_all(cfg,buildtype,platform,mcc::config::ExportType::EXPORT_NONE,cfg->version);
                    if (code) {
                        res.set_fail(std::format("Failed to package mcc {} for {}",mcc::compiler::build_type_names[int(buildtype)],mcc::compiler::platform_names[int(platform)]));

                        return;
                    }
                }
            }
        });
        if (code) return;

        res.set_success();
    });
}
