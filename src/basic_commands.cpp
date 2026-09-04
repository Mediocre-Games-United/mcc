#include "basic_commands.hpp"
#include "logger.hpp"
#include "commands.hpp"
#include <cstdint>
#include <format>

static uint8_t create_package_optimized();
static uint8_t run_program();
static uint8_t list_all_commands();

void mcc::basic_commands::init() {
    mcc::add_command<>(U"help",&list_all_commands);

    mcc::add_command<>(U"package",&create_package_optimized);
    mcc::add_command<>(U"run",&run_program);

    baseutils::log_success("Initialized basic commands");
}

static uint8_t list_all_commands() {
    auto commands = mcc::get_commands();
    for (auto &cmd : commands) {
        baseutils::cli_output(std::format("{}",cmd.help_text));
    }

    return 0;
}

static bool has_config() {
    return false;
}
static uint8_t create_package_optimized() {
    if (!has_config()) {
        baseutils::log_error(false,"No config found. Generate one with `config`");

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
