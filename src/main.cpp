#include "background.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "logger.hpp"
#include "basic_commands.hpp"

static bool is_running = true;
int main() {
    cbu::log_info("Starting program");

    mcc::basic_commands::init();
    mcc::start_background();

    cbu::log_success("Entering main loop mode");

    string cmd;
    while (is_running) {
        cbu::cli_input("Enter command (help for help)");
        cmd = cbu::cli_get_string();

        uint8_t code = mcc::run_command(cmd);
        if (code == 137) {
            cbu::log_debug("Quit signal received");
            is_running = false;
        }
    }
    cbu::log_info("Gracefully closing program");

    mcc::end_background();
    cbu::log_success("Closed succesfully");
}
