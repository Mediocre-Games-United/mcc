#include "cpp-baseutils/include/logger.hpp"
#include <iostream>
#include "basic_commands.hpp"

static bool is_running = true;
int main() {
    baseutils::log_info("Starting program");

    mcc::basic_commands::init();
    baseutils::log_success("Entering main loop mode");

    std::string cmd;
    while (is_running) {
        baseutils::log_input("Enter command (help for help)");
        std::cin >> cmd;
    }
}
