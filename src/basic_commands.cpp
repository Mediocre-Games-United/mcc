#include "basic_commands.hpp"
#include "cpp-baseutils/include/logger.hpp"
#include "src/commands.hpp"
#include <cstdint>
#include <format>

static uint8_t create_package_optimized();
static uint8_t run_program();

void mcc::basic_commands::init() {
    mcc::add_command<>("package",&create_package_optimized);
    mcc::add_command<>("run",&run_program);

    baseutils::log_success("Initialized basic commands");
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
