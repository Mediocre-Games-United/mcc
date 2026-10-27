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
#include <unordered_set>

static U8 push_itchio(mcc::config::ConfigObject *cfg,mcc::version::Version version,mcc::compiler::Platform pt,mcc::compiler::BuildType tp) {
    fpath export_path = mcc::compiler::get_export_path(cfg,mcc::config::ExportType::EXPORT_ITCHIO,tp,pt,version);

    cbu::log_info(std::format("itchio export: {}",cbu::path_to_utf8(export_path)));

    string cmd = std::format("butler push ./export/ {}/{}:{}-{} --userversion \"{}\"",cfg->itchio_username,cfg->itchio_project,mcc::compiler::platform_names[int(pt)],mcc::compiler::build_type_names[int(tp)],mcc::version::get_version_string(version,"."));
    U8 code = cbu::run_shell_command(export_path,cmd,NULL);
    if (code) {
        cbu::log_error(false,"Itch.io push failed!");
        return -1;
    }
    cbu::log_success("Succesfully pushed");

    return 0;
}
inline string get_github_pkg_display(mcc::config::ConfigObject *cfg,mcc::version::Version version,mcc::compiler::Platform pt) {
    return std::format("{}-release-{}-{}-x86_64.tar.gz",cfg->get_raw_name(),mcc::version::get_version_string(version,"."),mcc::compiler::platform_names[int(pt)]);
}
static U8 publish_github(mcc::config::ConfigObject *cfg,mcc::version::Version version,vector<mcc::config::ExportClass> exp) {
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
        cbu::cli_input("Overwrite the existing release? (default false)");
        bool ans;
        cbu::cli_get_valid_bool(&ans,true);
        if (!ans) return -1;

        code = cbu::run_shell_command(cfg->directory,std::format("gh release delete {} --yes",tag),&output);
        if (code) {
            cbu::log_error(false,"Failed to delete old release");

            return -1;
        }
    }
    cbu::log_info("Creating the release");
    code = cbu::run_shell_command(cfg->directory,std::format("gh release create {} --title {} --generate-notes --draft",tag,tag),&output);
    if (code) {
        cbu::log_error(false,"Failed to create the github release");

        return -1;
    }

    string pkgs;
    string tmp = cbu::path_to_utf8(mcc::compiler::get_temp_path(cfg));
    std::error_code ec;
    for (auto &e : exp) {
        fpath f = mcc::compiler::get_export_path(cfg,mcc::config::ExportType::EXPORT_GITHUB,e.buildtype,e.platform,version) / "github_archive.tar.gz";

        cbu::log_debug(cbu::path_to_utf8(f));
        string nm = get_github_pkg_display(cfg,version,e.platform);
        std::filesystem::copy_file(f,std::format("{}/{}",tmp,nm),std::filesystem::copy_options::overwrite_existing,ec);
        if (ec) cbu::log_warn(ec.message());

        pkgs += std::format(" {}/{}#'{}'",tmp,nm,nm);
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

static vector<mcc::config::ExportClass> get_exps(mcc::config::ConfigObject *cfg,mcc::config::ExportType tp) {
    vector<mcc::config::ExportClass> exps{};
    for (auto &exp : cfg->export_types) {
        if (exp.type != tp) continue;

        exps.push_back(exp);
    }

    return exps;
}
static U8 push_itchio_all(mcc::config::ConfigObject *cfg,mcc::version::Version ver) {
    for (auto &c : get_exps(cfg,mcc::config::ExportType::EXPORT_ITCHIO)) {
        U8 code = push_itchio(cfg,ver,c.platform,c.buildtype);
        if (code) return code;
    }

    return 0;
}

U8 mcc::publisher::publish_all(mcc::config::ConfigObject *cfg,bool noconfirm) {
    U8 code = mcc::packager::export_all(cfg);
    if (code) return code;

    auto &version = cfg->version;
    bool confirm;
    std::unordered_set<mcc::config::ExportType> unique{};
    for (auto &exp : cfg->export_types) {
        unique.insert(exp.type);
    }

    if (unique.contains(mcc::config::ExportType::EXPORT_ITCHIO)) {
        if (!noconfirm) {
            cbu::cli_input("Publish to itch.io? (default false)");
            cbu::cli_get_valid_bool(&confirm,true);
            if (confirm) code = push_itchio_all(cfg,version);
        } else code = push_itchio_all(cfg,version);
    }
    if (unique.contains(mcc::config::ExportType::EXPORT_GITHUB)) {
        if (!noconfirm) {
            cbu::cli_input("Publish to github? (default false)");
            cbu::cli_get_valid_bool(&confirm,true);
            if (confirm) code = publish_github(cfg,version,get_exps(cfg,mcc::config::ExportType::EXPORT_GITHUB));
        } else code = publish_github(cfg,version,get_exps(cfg,mcc::config::ExportType::EXPORT_GITHUB));
    }

    if (code) return code;

    return 0;
}
