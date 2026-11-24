#include "config_external_objects.hpp"
#include "archive.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "config_file.hpp"
#include "compiler.hpp"
#include "installer.hpp"
#include "shell.hpp"

using namespace mcc::config;

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
            archive_path,
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
    auto archive = ArchiveTarGZ();
    if (!archive.extract_archive_single(archive_path,extract_path)) {
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
    fpath selected_include;

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

    fpath atld = extract_path / binary->tld;
    cbu::cli_input(
        std::format(
            "Select the {} package include directory",
            platform_name
        )
    );
    if (!cbu::cli_get_valid_dirpath(
        &selected_include,
        atld
    )) {
        cbu::log_warn("Cancelled");
        return false;
    }

    binary->include_path =
    std::filesystem::relative(
        selected_include,
        atld
    ).generic_string();

    if (binary->include_path.empty()) {
        binary->include_path = "./";
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

    obj->win_ext_binary.dll_name =
    cbu::cli_get_string();

    if (!obj->win_ext_binary.dll_name.empty()) {
        cbu::cli_input(
            "Enter the import library (lib[name].dll.a) name"
        );

        obj->win_ext_binary.bin_name =
        cbu::cli_get_string();
        if (obj->win_ext_binary.bin_name.empty()) {
            cbu::log_error(false,"Import library cannot be empty");
            return false;
        }
    }

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

void mcc::config_commands::config_external_cmd() {
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

    bool edit_running = true;
    while (edit_running) {
        cbu::cli_output("Welcome to the external object utility");
        cbu::cli_input("What to do?\nExit external object utility (0 default)\nAdd new external object (1)");
        int opt;
        cbu::cli_get_valid_int(&opt,0,1,true,0);
        if (mcc::consume_interrupt()) break;

        switch (opt) {
            case 0: {
                edit_running = false;
                break;
            } case 1: {
                ExternalObject obj{};

                cbu::cli_input("Enter external object name");

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
                        ExternalObject(std::move(obj))
                    );

                    update_config(cfg);

                break;
            }
        }
    }
}
