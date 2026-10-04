#include "background.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "command_parser.hpp"
#include "commands.hpp"
#include "config_file.hpp"
#include "handlers.hpp"
#include "init.hpp"
#include "logger.hpp"
#include "basic_commands.hpp"
#include "state.hpp"
#include "workers.hpp"
#include <filesystem>
#include <format>

volatile size_t mcc::interrupt_count = 0;
std::mutex mcc::interrupt_mutex{};
std::mutex cbu::cli_mutex;

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

        U8 code = mcc::run_command(cmd);
        if (code == 137) {
            cbu::log_debug("Quit signal received");
            is_running = false;
        }
        if (mcc::consume_interrupt()) is_running = false;
    }
    cbu::log_info("Gracefully closing program");
}
static int handle_parser_output(cbu::parser_output pout) {
    if (pout.values.contains('n')) {
        bool r = mcc::config::set_name_current(pout.values['n']);
        if (!r) {
            cbu::log_error(false,"Name not found!");
            return -1;
        }
    } else if (pout.values.contains('c')) {
        bool r = mcc::config::set_file_current(pout.values['c']);
        if (!r) {
            cbu::log_error(false,"File not found!");
            return -1;
        }
    } else if (pout.flags.contains('t')) {
        auto temp = new mcc::config::ConfigObject();
        temp->is_temp = true;

        temp->directory = std::filesystem::current_path();
        temp->src_directory = "src";
        temp->model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
        temp->name = "temp";
        temp->export_types.push_back(mcc::config::ExportType::EXPORT_DEFAULT);

        mcc::config::update_config(temp);
        mcc::state::state_safe([temp]() {
            mcc::state::active_config = temp;
            mcc::state::current_project = temp->directory;
        });
    } else {
        if (std::filesystem::exists(pout.cmd)) {
            if (!mcc::config::set_file_current(pout.cmd)) {
                cbu::log_error(false,"Could not activate file!");
                return -1;
            }
            pout.cmd = "";
        }
    }

    if (pout.cmd.empty()) {
        enter_interactive();
        return 0;
    } else {
        return standard_mode(pout.cmd);
    }

    return 0;
}

int main(int argc,char *argv[]) {
    int exit = 0;
    cbu::set_interrupt_handler(mcc::interrupt_count);

    cbu::log_debug(std::format("argc: {}",argc));
    for (int i = 0; i < argc; i ++) {
        cbu::log_debug(std::format("argv[{}]: '{}'",i,argv[i]));
    }

    cbu::log_info("Starting program");
    cbu::init();

    mcc::basic_commands::init();
    mcc::start_background();

    cbu::parser_output pout = cbu::parse_args(argc,argv,{
        cbu::parser_flag('t',"temp","Generates a temporary config in the current working directory for monolithic executables that require no linking steps"),
    },{
        cbu::parser_val(cbu::parser_flag('n',"config-name","Same as --config but set the config by name rather than filepath")),
        cbu::parser_val(cbu::parser_flag('c',"config","Sets the current config filepath, if not present the current working directory will be used instead")),
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
