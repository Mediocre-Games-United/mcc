#include "basic_commands.hpp"
#include "config/config_file.hpp"
#include "logger.hpp"
#include "commands.hpp"
#include <cstdint>
#include <format>

static uint8_t create_package_optimized();
static uint8_t run_program();
static uint8_t list_all_commands();

void mcc::basic_commands::init() {
    mcc::add_command<>("help",&list_all_commands,{},"Displays this help menu :woah:");
    mcc::add_command<>("quit",(uint8_t(*)()) NULL,{},"Quits the program (shocker!!)");

    mcc::add_command<>("config",&mcc::config::cmd,{},"Opens the config menu for creating, editing & selecting configs.");
    mcc::add_command<>("package",&create_package_optimized,{},"Creates a ready to run & distribute package. Will skip as many steps as possible and not rebuild unless source files have changed");
    mcc::add_command<>("run",&run_program,{},"Runs the package command and starts the executable.");

    baseutils::log_success("Initialized basic commands");

    mcc::config::init();
    baseutils::log_success("Initialized config");
}

static uint8_t list_all_commands() {
    auto commands = mcc::list_commands();
    for (auto &cmd : commands) {
        baseutils::cli_output(std::format("[{}] {}\n",cmd.name,cmd.help_example));
    }

    return 0;
}

static uint8_t create_package_optimized() {
    if (!mcc::config::valid()) {
        baseutils::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }

    return 0;
}
static uint8_t run_program() {
    uint8_t res = create_package_optimized();
    if (res) {
        baseutils::log_error(false,std::format("Failed to create package, exit code {}",res));
        return res;
    } else baseutils::log_verbose("Package created, starting the run wrapper");


    return 0;
}
