#include "installer.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include <filesystem>
#include <format>

static uint8_t install_linux_package(mcc::config::ExternalPackage pck) {
    uint8_t code;
    string output;

    string test_cmd,install_cmd = "";
    if (std::filesystem::exists("/usr/bin/pacman")) {
        test_cmd = std::format("pacman -Q {}",pck.name);
        install_cmd = std::format("sudo pacman -Sy {}",pck.name);
    }
    if (std::filesystem::exists("/usr/bin/apt-get")) {
        test_cmd = std::format(
            "dpkg-query -W -f='${{Status}}' {} 2>/dev/null | "
            "grep -q '^install ok installed$'",
            pck.name
        );
        install_cmd = std::format("sudo pacman -Sy {}",pck.name);
    }

    if (test_cmd.empty()) {
        cbu::log_error(false,"No supported package managers found");

        return -1;
    }
    code = cbu::run_shell_command("/",test_cmd,&output);
    if (code) code = cbu::run_shell_command("/",install_cmd,NULL);
    else cbu::log_info(std::format("{} is already installed",pck.name));

    if (code) {
        cbu::log_error(false,std::format("Failed to install {}",pck.name));

        return -1;
    }

    return 0;
}

uint8_t mcc::installer::install_all(mcc::config::ConfigObject *cfg) {
    cbu::log_info("Installing config...");
    uint8_t code;

    for (auto &s : cfg->sub_projects) {
        cbu::log_info("Installing subconfig first");
        code = install_all(s);
        if (code) return code;
    }
    for (auto &ext : cfg->external_objects) {
        if (ext->linux_package) {
            code = install_linux_package(ext->linux_package);
            if (code) return code;
        } else {
            cbu::log_error(false,"Not implemented");
        }
    }

    cbu::log_success("Config installed succesfully");
    return 0;
}
