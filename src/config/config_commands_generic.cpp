#include "config_commands_generic.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "config_file.hpp"
#include "logger.hpp"

using namespace mcc::config;

void mcc::config_commands::list_configs_cmd() {
    if (!mcc::config::current_config or mcc::config::current_config->loaded_configs.empty()) {
        cbu::log_warn("No configs found! Add some with a or create a new one with n");
        return;
    }
    for (auto &s : mcc::config::current_config->loaded_configs) {
        cbu::cli_output(std::format("---\n{} at {}",s->name,cbu::path_to_utf8(s->directory)));
    }
}
void mcc::config_commands::link_config_cmd() {
    fpath path;
    cbu::cli_input("Enter the project path to link");
    if (!cbu::cli_get_valid_dirpath(&path)) {
        cbu::log_error(false,"Cancelled");
        return;
    }

    fpath ppath = path / FNAME;
    if (!std::filesystem::exists(ppath)) {
        cbu::log_warn("Project has no config file!");
        return;
    } else {
        if (!std::filesystem::is_regular_file(ppath)) {
            cbu::log_warn("Project is not a file");
            return;
        }
        link_config_by_file(ppath);

        return;
    }
}

void mcc::config_commands::edit_config_cmd() {
    string name;
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

    bool edit_running = true;
    while (edit_running) {
        cbu::cli_output("Welcome to the config edit utility");
        cbu::cli_input("What to edit?\nExit edit utility (0)\nName (1 default)\nSrc Directory (2)\nExternal Objects (3)");
        int opt;
        cbu::cli_get_valid_int(&opt,0,3,true,1);
        if (mcc::consume_interrupt()) break;
        switch (opt) {
            case 0: {
                edit_running = false;
                break;
            }
            case 1: {
                string name;
                cbu::cli_input("Enter new name");
                if (!cbu::cli_get_valid_string(&name)) {
                    cbu::log_warn("Name cannot be empty");
                    break;
                }
                cfg->name = name;
                update_config(cfg);

                break;
            }
            case 2: {
                fpath dir;
                cbu::cli_input("Enter source directory");
                if (!cbu::cli_get_valid_dirpath(&dir,cfg->directory)) {
                    cbu::log_warn("Canceled");
                    break;
                }
                cfg->src_directory = std::filesystem::relative(dir,cfg->directory);
                update_config(cfg);

                break;
            }
            case 3: {
                bool remove;
                cbu::cli_input("Remove all external objects? (default false)");
                if (!cbu::cli_get_valid_bool(&remove,true,false)) {
                    cbu::log_warn("Canceled");
                    break;
                }
                if (!remove) break;
                cbu::log_warn("Removing all ext objects!");
                cfg->external_objects = {};
                update_config(cfg);

                break;
            }
        }
    }
}
void mcc::config_commands::new_config_cmd() {

    string name;
    ConfigModel model;
    fpath project_path;
    fpath src_path;

    cbu::cli_input("Enter config name");
    if (!cbu::cli_get_valid_string(&name)) {
        cbu::log_error(false,"Name cannot be empty");
        return;
    }
    int mi;
    cbu::cli_input("---\nEnter config mode\nGeneric executable (0 default)\nShared library (1)\nExecutable with subprojects (2)\nEnter value");
    if (!cbu::cli_get_valid_int(&mi,0,2,true,0)) {
        cbu::log_error(false,"Invalid value");
        return;
    }
    switch (mi) {
        case 0: {
            model = ConfigModel::SINGLE_EXECUTABLE;
            break;
        } case 1: {
            model = ConfigModel::SINGLE_SHARED;
            break;
        } case 2: {
            model = ConfigModel::EXECUTABLES_WITH_SHARED;
            break;
        }
    }


    cbu::cli_output("---\nEnter the project path");
    if (!cbu::cli_get_valid_dirpath(&project_path)) {
        cbu::log_error(false,"Cancelled");
        return;
    }

    cbu::cli_output("---\nEnter the path for source files");
    if (!cbu::cli_get_valid_dirpath(&src_path,project_path)) {
        cbu::log_error(false,"Cancelled");
        return;
    }
    src_path = std::filesystem::relative(src_path,project_path);

    config::ConfigObject *cfg = new config::ConfigObject();
    cbu::log_debug(std::format("Creating config {} at path {} with src {}",
                               name,
                               cbu::path_to_utf8(project_path),
                               cbu::path_to_utf8(src_path)));
    cfg->name = name;
    cfg->directory = project_path;
    cfg->src_directory = src_path;
    cfg->model = model;
    update_config(cfg);

    link_config(cfg);
    cbu::log_success("Config created and linked succesfully!");
}
