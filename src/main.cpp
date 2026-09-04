#include "background.hpp"
#include "base_types.hpp"
#include "commands.hpp"
#include "logger.hpp"
#include <iostream>
#include "basic_commands.hpp"

static bool is_running = true;
int main() {
    baseutils::log_info("Starting program");

    mcc::basic_commands::init();
    mcc::start_background();

    baseutils::log_success("Entering main loop mode");

    string cmd;
    while (is_running) {
        baseutils::cli_input("Enter command (help for help)");
        cmd = baseutils::cli_get_string();

        uint8_t code = mcc::run_command(cmd);
        if (code == 137) {
            baseutils::log_debug("Quit signal received");
            is_running = false;
        }
    }
    baseutils::log_info("Gracefully closing program");

    mcc::end_background();
    baseutils::log_success("Closed succesfully");
}
