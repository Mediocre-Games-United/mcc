#include "config_commands_special.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "installer.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include <format>

using namespace mcc::config;

void mcc::config_commands::config_parent_dir_cmd() {
    string name;
    fpath parentpath;

    cbu::cli_input("Enter config name");
    if (!cbu::cli_get_valid_string(&name)) {
        cbu::log_error(false,"Name cannot be empty");
        return;
    }
    ConfigObject *cfg = find_config_by_name(name);
    if (!cfg) {
        cbu::log_warn("Config does not exist or is not linked!");
        return;
    }
    cbu::cli_input("Enter parent path");
    if (!cbu::cli_get_valid_dirpath(&parentpath,cfg->directory)) {
        cbu::log_error(false,"Cancelled");
        return;
    }

    fpath relpath = std::filesystem::relative(parentpath,cfg->directory);
    cbu::log_debug(std::format("Relative path: {}",cbu::path_to_utf8(relpath)));

    cfg->has_parent_directory = true;
    cfg->parent_directory = relpath;

    update_config(cfg);

    cbu::log_success("Added parent directory succesfully!");
}

void mcc::config_commands::config_add_export_cmd() {
    string name;
    fpath parentpath;

    cbu::cli_input("Enter config name");
    if (!cbu::cli_get_valid_string(&name)) {
        cbu::log_error(false,"Name cannot be empty");
        return;
    }
    auto cfg = find_config_by_name(name);
    if (!cfg) {
        cbu::log_warn("Config does not exist or is not linked!");
        return;
    }
    cbu::cli_input("Enter export type\nDefault (0 default)\nItch.io (1)");
    int opt;
    if (!cbu::cli_get_valid_int(&opt,0,1,true,0)) {
        cbu::log_error(false,"Invalid value");
        return;
    }
    ExportType exp;
    switch (opt) {
        case 0: {
            exp = ExportType::EXPORT_DEFAULT;
            break;
        } case 1: {
            exp = ExportType::EXPORT_ITCHIO;

            string username,project;
            cbu::cli_input("Enter Itch.io username");
            if (!cbu::cli_get_valid_string(&username)) {
                cbu::log_error(false,"Username cannot be empty");
                return;
            }
            cbu::cli_input("Enter Itch.io project name");
            if (!cbu::cli_get_valid_string(&project)) {
                cbu::log_error(false,"Project cannot be empty");
                return;
            }

            cfg->itchio_username = username;
            cfg->itchio_project = project;
            update_config(cfg);

            break;
        }
    }
    bool match = false;
    for (auto &s : cfg->export_types) {
        if (s != exp) continue;

        match = true;
        break;
    }
    cbu::log_success("Added export type to config!");
    if (match) return;

    cfg->export_types.push_back(exp);
    update_config(cfg);
}



struct BinarySetup {
    const char *platform_name;
    const char *archive_name;
    const char *archive_url_prompt;
    const char *binary_name_prompt;
};

bool download_and_extract_binary(
    ConfigObject *cfg,
    const std::string &platform_name,
    const std::string &archive_name,
    const std::string &url,
    ExternalBinary *binary
) {
    const fpath temp_path =
    mcc::compiler::get_temp_path(cfg);

    const fpath archive_path =
    temp_path / archive_name;

    const fpath extract_path =
    temp_path / (platform_name + "_external_archive");

    std::filesystem::remove(archive_path);
    std::filesystem::remove_all(extract_path);
    std::filesystem::create_directories(extract_path);

    cbu::cli_output(
        std::format("Downloading {} archive...", platform_name)
    );

    const U8 download_code = cbu::run_shell_command(
        temp_path,
        std::format(
            "curl --fail --location --output \"{}\" \"{}\"",
            archive_path.string(),
                    url
        ),
        nullptr
    );

    if (download_code != 0) {
        cbu::log_error(
            false,
            std::format(
                "Downloading {} archive failed, is the link invalid?",
                platform_name
            )
        );
        return false;
    }

    cbu::cli_output(
        std::format("Extracting {} archive...", platform_name)
    );

    const U8 extract_code = cbu::run_shell_command(
        temp_path,
        std::format(
            "tar -xzf \"{}\" -C \"{}\"",
            archive_path.string(),
                    extract_path.string()
        ),
        nullptr
    );

    if (extract_code != 0) {
        cbu::log_error(
            false,
            std::format(
                "Extracting {} archive failed",
                platform_name
            )
        );
        return false;
    }

    /*
     * tld is the directory containing include/, lib/, and/or bin/.
     *
     * For example, if the archive extracts to:
     *
     *   package-1.2.3/include/foo.h
     *   package-1.2.3/lib/libfoo.so
     *
     * the user should select package-1.2.3, and tld becomes:
     *
     *   package-1.2.3
     */
    fpath selected_tld;

    cbu::cli_input(
        std::format(
            "Select the {} package top-level directory",
            platform_name
        )
    );

    if (!cbu::cli_get_valid_dirpath(
        &selected_tld,
        extract_path
    )) {
        cbu::log_warn("Cancelled");
        return false;
    }

    binary->tld =
    std::filesystem::relative(
        selected_tld,
        extract_path
    ).generic_string();

    if (binary->tld.empty()) {
        binary->tld = "./";
    }

    return true;
}

bool configure_linux_package(ExternalObject *obj) {
    cbu::cli_input(
        "Package name in Linux package managers "
        "(leave empty if unavailable)"
    );

    obj->linux_package.name = cbu::cli_get_string();

    if (obj->linux_package.name.empty()) {
        return true;
    }

    cbu::cli_input(
        "Enter the shared library name "
        "(leave empty for header-only)"
    );

    obj->linux_package.link_name = cbu::cli_get_string();

    cbu::log_info(std::format("Package name {}, link {}",obj->linux_package.name,obj->linux_package.link_name));

    return true;
}

bool configure_linux_binary(
    ConfigObject *cfg,
    ExternalObject *obj
) {
    cbu::cli_input(
        "URL for the Linux tar.gz download "
        "(leave empty if unavailable)"
    );

    obj->linux_ext_binary.download_url =
    cbu::cli_get_string();

    if (!obj->linux_ext_binary) {
        return true;
    }

    cbu::cli_input(
        "Enter the shared library name "
        "(leave empty for header-only)"
    );

    obj->linux_ext_binary.bin_name =
    cbu::cli_get_string();

    cbu::cli_input(
        "Optional install command to run after extraction "
        "(leave empty for none)"
    );

    obj->linux_ext_binary.install_cmd =
    cbu::cli_get_string();

    return download_and_extract_binary(
        cfg,
        "Linux",
        "linux_external_archive.tar.gz",
        obj->linux_ext_binary.download_url,
        &obj->linux_ext_binary
    );
}
bool configure_windows_binary(
    ConfigObject *cfg,
    ExternalObject *obj
) {
    cbu::cli_input(
        "URL for the MinGW tar.gz download for Windows "
        "(such as a GitHub release; leave empty for none)"
    );

    obj->win_ext_binary.download_url =
    cbu::cli_get_string();

    if (!obj->win_ext_binary) {
        return true;
    }

    cbu::cli_input(
        "Enter the DLL name "
        "(leave empty for header-only)"
    );

    obj->win_ext_binary.bin_name =
    cbu::cli_get_string();

    cbu::cli_input(
        "Optional install command to run after extraction "
        "(executed in the package top-level directory; "
        "leave empty for none)"
    );

    obj->win_ext_binary.install_cmd =
    cbu::cli_get_string();

    return download_and_extract_binary(
        cfg,
        "Windows",
        "windows_external_archive.tar.gz",
        obj->win_ext_binary.download_url,
        &obj->win_ext_binary
    );
}

void mcc::config_commands::config_add_external_cmd() {
    string config_name;

    cbu::cli_input("Enter config name");

    if (!cbu::cli_get_valid_string(&config_name)) {
        cbu::log_error(false, "Name cannot be empty");
        return;
    }

    ConfigObject *cfg =
    find_config_by_name(config_name);

    if (!cfg) {
        cbu::log_warn(
            "Config does not exist or is not linked!"
        );
        return;
    }

    ExternalObject obj{};

    cbu::cli_input("Enter external package name");

    if (!cbu::cli_get_valid_string(&obj.name)) {
        cbu::log_error(false, "Name cannot be empty");
        return;
    }

    /*
     * Prefer a Linux package-manager package. Only ask for a
     * downloadable Linux binary when no package was supplied.
     */
    configure_linux_package(&obj);

    if (!obj.linux_package) {
        if (!configure_linux_binary(cfg, &obj)) {
            return;
        }
    }

    if (!configure_windows_binary(cfg, &obj)) {
        return;
    }

    if (!obj.linux_package &&
        !obj.linux_ext_binary &&
        !obj.win_ext_binary) {
        cbu::log_error(
            false,
            "No Linux package, Linux archive, or Windows archive "
            "was configured"
        );
    return;
        }

        cbu::log_info("External object adding, trying to install it...");
        cfg->external_objects.push_back(
            new ExternalObject(std::move(obj))
        );

        update_config(cfg);
        mcc::installer::install_all(cfg);
}
