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
#include <filesystem>
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
    cbu::log_success("Succesfully pushed");

    return 0;
}
inline string get_github_pkg_display(mcc::config::ConfigObject *cfg,mcc::version::Version version,mcc::compiler::Platform pt) {
    return std::format("{}-release-{}-{}-x86_64.tar.gz",cfg->get_raw_name(),mcc::version::get_version_string(version,"."),mcc::compiler::platform_names[int(pt)]);
}
static U8 publish_github(mcc::config::ConfigObject *cfg,mcc::version::Version version) {
    cbu::log_info("Finding repo");
    string output;
    U8 code;

    string tag = mcc::version::get_version_string(version,".");

    code = cbu::run_shell_command(cfg->directory,"gh repo view",&output);
    if (code) {
        cbu::log_error(false,"No github repository found!");
        return -1;
    }
    code = cbu::run_shell_command(cfg->directory,std::format("gh release view {}",tag),&output);
    if (!code) {
        cbu::log_error(false,"Github release for this version already exists!");
        return -1;
    }
    cbu::log_info("Creating the release");
    code = cbu::run_shell_command(cfg->directory,std::format("gh release create {} --title {} --generate-notes --draft",tag,tag),&output);
    if (code) {
        cbu::log_error(false,"Failed to create the github release");

        return -1;
    }

    std::error_code ec;
    fpath f = mcc::compiler::get_export_path(cfg,mcc::config::ExportType::EXPORT_GITHUB,mcc::compiler::BuildType::BUILD_RELEASE,mcc::compiler::Platform::PLATFORM_LINUX,version) / "github_archive.tar.gz";
    string nm = get_github_pkg_display(cfg,version,mcc::compiler::Platform::PLATFORM_LINUX);
    std::filesystem::copy_file(f,std::format("/tmp/{}",nm),std::filesystem::copy_options::overwrite_existing,ec);

    string pkgs = std::format("/tmp/{}#'{}'",nm,nm);
    if (!cfg->github_no_windows) {
        fpath f = mcc::compiler::get_export_path(cfg,mcc::config::ExportType::EXPORT_GITHUB,mcc::compiler::BuildType::BUILD_RELEASE,mcc::compiler::Platform::PLATFORM_WINDOWS,version) / "github_archive.tar.gz";
        string nm = get_github_pkg_display(cfg,version,mcc::compiler::Platform::PLATFORM_WINDOWS);
        std::filesystem::copy_file(f,std::format("/tmp/{}",nm),std::filesystem::copy_options::overwrite_existing,ec);

        pkgs = std::format("{} /tmp/{}#'{}'",pkgs,nm,nm);
    }

    cbu::log_info("Uploading files");
    code = cbu::run_shell_command(cfg->directory,std::format("gh release upload {} {}",tag,pkgs),&output);
    if (code) {
        cbu::log_error(false,"Failed to upload the packages");

        return -1;
    }

    cbu::log_success("Draft release created");
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
        } else code = push_itchio(cfg,version);
    }
    if (cbu::vector_has_value(cfg->export_types,mcc::config::ExportType::EXPORT_GITHUB)) {
        if (!noconfirm) {
            cbu::cli_input("Publish to github? (default false)");
            cbu::cli_get_valid_bool(&confirm,true);
            if (confirm) code = publish_github(cfg,version);
        } else code = publish_github(cfg,version);
    }

    if (code) return code;

    return 0;
}
