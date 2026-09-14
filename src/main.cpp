#include "background.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "config/config_file.hpp"
#include "init.hpp"
#include "logger.hpp"
#include "basic_commands.hpp"
#include "workers.hpp"

static bool is_running = true;
int standard_mode(int argc,char *argv[]) {
    cbu::log_error(false,"Standard mode not implemented yet!");

    return 0;
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

    if (argc <= 1) enter_interactive();
    else {
        string arg1 = argv[1];
        fpath test = fpath(arg1);
        if (test.is_absolute()) { // open file mode!
            cbu::log_info("Activating config...");
            mcc::config::set_file_current(test);

            enter_interactive();
        } else { // standard mode
            exit = standard_mode(argc,argv);
        }
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
