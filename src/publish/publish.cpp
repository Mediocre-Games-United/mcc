#include "publish.hpp"
#include "cli.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "packager.hpp"
#include "shell.hpp"
#include "vectormath.hpp"
#include "version.hpp"
#include <format>

static vector<mcc::compiler::Platform> export_platforms = {mcc::compiler::Platform::PLATFORM_LINUX,mcc::compiler::Platform::PLATFORM_WINDOWS};
static vector<mcc::compiler::BuildType> export_types = {mcc::compiler::BuildType::BUILD_BETA,mcc::compiler::BuildType::BUILD_RELEASE};

static U8 push_itchio(mcc::config::ConfigObject *cfg,mcc::version::Version version) {
    for (auto &pt : export_platforms) {
        for (auto &tp : export_types) {
            fpath export_path = mcc::compiler::get_export_path(cfg,mcc::config::ExportType::EXPORT_ITCHIO,tp,pt,version);

            cbu::log_info(std::format("itchio export: {}",cbu::path_to_utf8(export_path)));

            string cmd = std::format("butler push ./export/ {}/{}:{}-{} --userversion \"{}\"",cfg->itchio_username,cfg->itchio_project,mcc::compiler::platform_names[int(pt)],mcc::compiler::build_type_names[int(tp)],mcc::version::get_version_string(version,"."));
            U8 code = cbu::run_shell_command(export_path,cmd,NULL);
            if (code) {
                cbu::log_error(false,"Itch.io push failed!");
                return -1;
            }
        }
    }

    return 0;
}
U8 mcc::publisher::publish_all(mcc::config::ConfigObject *cfg,bool noconfirm) {
    U8 code = mcc::packager::export_all(cfg);
    if (code) return code;

    auto version = cfg->version;
    bool confirm;
    if (cbu::vector_has_value(cfg->export_types,mcc::config::ExportType::EXPORT_ITCHIO)) {
        if (!noconfirm) {
            cbu::cli_input("Publish to itch.io? (default false)");
            cbu::cli_get_valid_bool(&confirm,true);
            if (confirm) code = push_itchio(cfg,version);
        } code = push_itchio(cfg,version);
    }

    if (code) return code;

    return 0;
}
