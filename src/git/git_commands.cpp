#include "git_commands.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "state.hpp"
#include <format>

static vector<string> separate_lines(string output) {
    vector<string> out{};
    string line = "";
    for (char c : output) {
        if (c == 0) {
            out.push_back(line);
            line = "";
            continue;
        }

        line += c;
    }

    return out;
}
static U8 log_no_conf() {
    cbu::log_error(false,"No config active");

    return -1;
}
static U8 log_no_git() {
    cbu::log_error(false,"Config has no git repository");

    return -1;
}

U8 mcc::git::sync() {
    return 0;
}
U8 mcc::git::undo() {
    return 0;
}
U8 mcc::git::status() {
    if (!mcc::config::valid()) return log_no_conf();

    string output;
    U8 code = 0;
    mcc::state::state_safe([&output,&code]() {
        if (cbu::run_shell_command(mcc::state::active_config->directory,"git status --porcelain=v2 --branch -z",&output)) {
            code = log_no_git();
        }
    });
    if (code) return log_no_git();

    string branch = "<unknown>";
    for (string line : separate_lines(output)) {
        if (line.starts_with("# branch.head ")) {
            branch = line.substr(14);
        }
    }

    cbu::cli_output(std::format("Current branch: {}",branch));

    return 0;
}
