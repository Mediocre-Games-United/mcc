#include "config_commands_special.hpp"
#include "archive.hpp"
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
    cbu::log_debug(std::format("Relative path: {}",relpath));

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
    cbu::cli_input("Enter export type\nDefault (0 default)\nItch.io (1)\nGithub.com (2)\nPortable (3)\nHeaders + libs (4)\nSingle file bundled executable (5)\nSingle file bundled installer (6)");
    int opt;
    if (!cbu::cli_get_valid_int(&opt,0,6,true,0)) {
        cbu::log_error(false,"Invalid value");
        return;
    }
    ExportType exp;
    switch (opt) {
        case 0: {
            exp = &export_type_default;
            break;
        } case 1: {
            exp = &export_type_itchio;

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
        case 2: {
            exp = &export_type_github;

            break;
        }
        case 3: {
            exp = &export_type_portable;

            break;
        }
        case 4: {
            exp = &export_type_shared_headers;

            break;
        }
        case 5: {
            exp = &export_type_bundle;

            break;
        }
        case 6: {
            exp = &export_type_installer;

            string name,readable;
            cbu::cli_input("Enter package name id (non readable)");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                return;
            }
            cbu::cli_input("Enter package display name");
            if (!cbu::cli_get_valid_string(&readable)) {
                cbu::log_error(false,"Name cannot be empty");
                return;
            }

            cfg->install_pkg_name = name;
            cfg->install_pkg_readable = readable;
            update_config(cfg);

            break;
        }
    }
    ExportClass c{};
    c.type = exp;

    int pt,tp;
#ifdef _WIN32
    cbu::cli_input("Enter export platform\nWindows (0 default)\nLinux (1)");
    if (!cbu::cli_get_valid_int(&pt,0,1,true,0)) {
#else
    cbu::cli_input("Enter export platform\nWindows (0)\nLinux (1 default)");
    if (!cbu::cli_get_valid_int(&pt,0,1,true,1)) {
#endif
        cbu::log_error(false,"Invalid value");
        return;
    }
    c.platform = mcc::compiler::get_platform_index(pt);

    cbu::cli_input("Enter export build type\nRelease (0)\nBeta (1 default)");
    if (!cbu::cli_get_valid_int(&tp,0,1,true,1)) {
        cbu::log_error(false,"Invalid value");
        return;
    }
    c.buildtype = mcc::compiler::get_build_type_index(tp);

    bool match = false;
    for (auto &s : cfg->export_types) {
        if (s != c) continue;

        match = true;
        break;
    }
    if (match) {
        cbu::log_warn("Export type already exists!");
        return;
    }

    cbu::log_success("Added export type to config!");
    cfg->export_types.push_back(c);
    update_config(cfg);
}



