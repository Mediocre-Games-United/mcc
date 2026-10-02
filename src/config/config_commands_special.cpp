#include "config_commands_special.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "installer.hpp"
#include "shell.hpp"

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
void mcc::config_commands::config_add_external_cmd() {
    string config_name;
    cbu::cli_input("Enter config name");

    if (!cbu::cli_get_valid_string(&config_name)) {
        cbu::log_error(false, "Name cannot be empty");
        return;
    }

    ConfigObject *cfg = find_config_by_name(config_name);
    if (!cfg) {
        cbu::log_warn("Config does not exist or is not linked!");
        return;
    }

    ExternalObject obj{};

    cbu::cli_input("Enter external package name");
    if (!cbu::cli_get_valid_string(&obj.name)) {
        cbu::log_error(false, "Name cannot be empty");
        return;
    }

    /*
     * Linux package-manager configuration.
     *
     * If no package name is supplied, a downloadable Linux archive can be
     * configured below instead.
     */
    cbu::cli_input(
        "Package name in Linux package managers "
        "(leave empty if unavailable)"
    );

    obj.linux_package.name = cbu::cli_get_string();

    if (!obj.linux_package.name.empty()) {
        cbu::cli_input(
            "Enter the shared library name "
            "(leave empty for header-only)"
        );
        obj.linux_package.link_name = cbu::cli_get_string();

        cbu::cli_input("Enter the include path");
        if (!cbu::cli_get_valid_string(&obj.linux_package.include_path)) {
            cbu::log_warn(
                "Empty include path, using /usr/include as a default..."
            );
            obj.linux_package.include_path = "/usr/include";
        }
    } else {
        /*
         * Linux binary fallback.
         */
        cbu::cli_input(
            "URL for the Linux tar.gz download "
            "(leave empty if unavailable)"
        );
        obj.linux_ext_binary.download_url = cbu::cli_get_string();

        if (!obj.linux_ext_binary.download_url.empty()) {
            cbu::cli_input(
                "Enter the shared library name "
                "(leave empty for header-only)"
            );
            obj.linux_ext_binary.bin_name = cbu::cli_get_string();

            cbu::cli_input(
                "Optional install command to run after extraction "
                "(leave empty for none)"
            );
            obj.linux_ext_binary.install_cmd = cbu::cli_get_string();

            cbu::cli_output("Downloading Linux archive...");

            const fpath temp_path =
            mcc::compiler::get_temp_path(cfg);

            const fpath archive_path =
            temp_path / "linux_external_archive.tar.gz";

            const fpath extract_path =
            temp_path / "linux_external_archive";

            std::filesystem::remove(archive_path);
            std::filesystem::remove_all(extract_path);
            std::filesystem::create_directories(extract_path);

            const uint8_t code = cbu::run_shell_command(
                temp_path,
                std::format(
                    "curl --fail --location --output {} {}",
                    archive_path.string(),
                            obj.linux_ext_binary.download_url
                ),
                nullptr
            );

            if (code != 0) {
                cbu::log_error(
                    false,
                    "Downloading Linux archive failed, is the link invalid?"
                );
                return;
            }

            cbu::cli_output("Extracting Linux archive...");

            if (cbu::run_shell_command(
                temp_path,
                std::format(
                    "tar -xzf {} -C {}",
                    archive_path.string(),
                            extract_path.string()
                ),
                nullptr
            ) != 0) {
                cbu::log_error(
                    false,
                    "Extracting Linux archive failed"
                );
                return;
            }

            fpath selected_include_path;
            cbu::cli_input("Select the Linux include path");

            if (!cbu::cli_get_valid_dirpath(
                &selected_include_path,
                extract_path
            )) {
                cbu::log_warn("Cancelled");
                return;
            }

            obj.linux_ext_binary.include_path =
            std::filesystem::relative(
                selected_include_path,
                extract_path
            ).string();
        }
    }

    /*
     * Windows binary configuration.
     */
    cbu::cli_input(
        "URL for the MinGW tar.gz download for Windows "
        "(such as a GitHub release; leave empty for none)"
    );

    obj.win_ext_binary.download_url = cbu::cli_get_string();

    if (!obj.win_ext_binary.download_url.empty()) {
        cbu::cli_input(
            "Enter the DLL name "
            "(leave empty for header-only)"
        );
        obj.win_ext_binary.bin_name = cbu::cli_get_string();

        cbu::cli_output("Downloading Windows archive...");

        const fpath temp_path =
        mcc::compiler::get_temp_path(cfg);

        const fpath archive_path =
        temp_path / "windows_external_archive.tar.gz";

        const fpath extract_path =
        temp_path / "windows_external_archive";

        std::filesystem::remove(archive_path);
        std::filesystem::remove_all(extract_path);
        std::filesystem::create_directories(extract_path);

        const uint8_t code = cbu::run_shell_command(
            temp_path,
            std::format(
                "curl --fail --location --output {} {}",
                archive_path.string(),
                        obj.win_ext_binary.download_url
            ),
            nullptr
        );

        if (code != 0) {
            cbu::log_error(
                false,
                "Downloading Windows archive failed, is the link invalid?"
            );
            return;
        }

        cbu::cli_output("Extracting Windows archive...");

        if (cbu::run_shell_command(
            temp_path,
            std::format(
                "tar -xzf {} -C {}",
                archive_path.string(),
                        extract_path.string()
            ),
            nullptr
        ) != 0) {
            cbu::log_error(
                false,
                "Extracting Windows archive failed"
            );
            return;
        }

        fpath selected_include_path;
        cbu::cli_input("Select the Windows include path");

        if (!cbu::cli_get_valid_dirpath(
            &selected_include_path,
            extract_path
        )) {
            cbu::log_warn("Cancelled");
            return;
        }

        obj.win_ext_binary.include_path =
        std::filesystem::relative(
            selected_include_path,
            extract_path
        ).string();
    }

    /*
     * At least one usable backend must be configured:
     *
     * - Linux package
     * - Linux downloadable archive
     * - Windows downloadable archive
     */
    if (!obj.linux_package &&
        !obj.linux_ext_binary &&
        !obj.win_ext_binary) {
        cbu::log_error(
            false,
            "No Linux package, Linux archive, or Windows archive was configured"
        );
    return;
        }

        cfg->external_objects.push_back(
            new ExternalObject(std::move(obj))
        );

        mcc::installer::install_all(cfg);
        update_config(cfg);
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
    cbu::cli_input("Enter export type\nDefault (0 default)");
    int opt;
    if (!cbu::cli_get_valid_int(&opt,0,0,true,0)) {
        cbu::log_error(false,"Invalid value");
        return;
    }
    ExportType exp;
    switch (opt) {
        case 0: {
            exp = ExportType::EXPORT_DEFAULT;
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
