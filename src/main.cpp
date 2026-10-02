#include "background.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "command_parser.hpp"
#include "commands.hpp"
#include "config/config_file.hpp"
#include "init.hpp"
#include "logger.hpp"
#include "basic_commands.hpp"
#include "workers.hpp"
#include <filesystem>
#include <format>


static bool is_running = true;
int standard_mode(string cmd) {
    cbu::log_verbose(std::format("CMD: '{}'",cmd));

    return mcc::run_command(cmd);
}
static void enter_interactive() {
    cbu::log_success("Entering interactive mode");

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
}
static int handle_parser_output(cbu::parser_output pout) {
    if (pout.cmd.empty()) {
        enter_interactive();
        return 0;
    }

    return 0;
}

int main(int argc,char *argv[]) {
    int exit = 0;
    cbu::log_debug(std::format("argc: {}",argc));
    for (int i = 0; i < argc; i ++) {
        cbu::log_debug(std::format("argv[{}]: '{}'",i,argv[i]));
    }

    cbu::log_info("Starting program");
    cbu::init();

    mcc::basic_commands::init();
    mcc::start_background();

    cbu::parser_output pout = cbu::parse_args(argc,argv,{
        cbu::parser_flag('t',"temp","Generates a temporary config for monolithic executables that require no linking steps"),
        cbu::parser_flag('c',"config","Sets the current config, if not present the current working directory will be used instead")
    },{

    });
    cbu::log_verbose(std::format("Parser returned {}",pout));


    if (pout.can_proceed and pout.valid) {
        exit = handle_parser_output(pout);
    } else if (!pout.valid) {
        cbu::log_error(false,pout.err);
    }

    mcc::end_background();
    cbu::deinit();
    cbu::log_success("Closed succesfully");
    return exit;
}

cbu::WorkerState *cbu::get_state() {
    static cbu::WorkerState wstate{};
    return &wstate;
}
