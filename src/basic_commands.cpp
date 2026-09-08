#include "basic_commands.hpp"
#include "cli.hpp"
#include "config/config_file.hpp"
#include "logger.hpp"
#include "commands.hpp"
#include "state.hpp"
#include "stringmath.hpp"
#include <cstdint>
#include <cstdlib>
#include <format>

static uint8_t create_package_optimized();
static uint8_t run_program();
static uint8_t list_all_commands();

void mcc::basic_commands::init() {
    mcc::add_command<>("help",&list_all_commands,{},"Displays this help menu :woah:");
    mcc::add_command<>("quit",(uint8_t(*)()) NULL,{},"Quits the program (shocker!!)");

    mcc::add_command<>("config",&mcc::config::cmd,{},"Opens the config menu for creating, editing & selecting configs.");
    mcc::add_command<>("reload",&mcc::config::reload,{},"Reload config settings & detect changes to files.");
    mcc::add_command<>("package",&create_package_optimized,{},"Creates a ready to run & distribute package. Will skip as many steps as possible and not rebuild unless source files have changed");
    mcc::add_command<>("run",&run_program,{},"Runs the package command and starts the executable.");

    cbu::log_success("Initialized basic commands");

    mcc::config::init();
    cbu::log_success("Initialized config");
}

static uint8_t list_all_commands() {
    auto commands = mcc::list_commands();
    for (auto &cmd : commands) {
        cbu::cli_output(std::format("[{}] {}\n",cmd.name,cmd.help_example));
    }

    return 0;
}

static void log_config(mcc::config::ConfigObject *obj) {
    if (!obj) cbu::log_error(true,"Active config is null!");
    cbu::log_info(std::format("Using config {}",obj->name));
}
static uint8_t create_package_optimized() {
    if (!mcc::config::valid()) {
        cbu::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }
    mcc::config::ConfigObject *cfg;
    mcc::state::state_safe([&cfg]() {
        cfg = cfg = mcc::state::active_config;
    });
    log_config(cfg);

    switch (cfg->model) {
        case mcc::config::ConfigModel::SINGLE_EXECUTABLE: {


            break;
        } default: {
            cbu::log_error(true,"Not implemented");
        }
    }

    return 0;
}
static uint8_t run_program() {
    uint8_t res = create_package_optimized();
    if (res) {
        cbu::log_error(false,std::format("Failed to create package, exit code {}",res));
        return res;
    } else cbu::log_verbose("Package created, starting the run wrapper");


    return 0;
}
