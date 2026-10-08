#include "git_commands.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "state.hpp"
#include "stringmath.hpp"
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
static U8 log_unsaved() {
    cbu::log_error(false,"Commit unsaved changes first!");

    return -1;
}

struct GitStatus {
    bool valid = true;

    umap<string,GitStatus> submodules;
    string branch = "<detached>";
    bool is_remote = false;
    bool has_unsaved = false;
    S64 ahead = 0;
    S64 behind = 0;
};
static GitStatus get_general_status(fpath dir) {
    GitStatus status{};

    string output;
    U8 code = 0;

    while (true) {
        if (cbu::run_shell_command(dir,"git fetch",&output)) {
            code = log_no_git();
        }
        else if (cbu::run_shell_command(dir,"git status --porcelain=v2 --branch -z",&output)) {
            code = log_no_git();
        }
        if (code) return {.valid = false};

        for (string line : separate_lines(output)) {
            cbu::log_debug(line);

            if (line.starts_with("# branch.head ")) {
                status.branch = line.substr(14);
            } else if (line.starts_with("# branch.ab ")) {
                status.is_remote = true;
                string full = line.substr(12);

                string f1 = cbu::string_split(full,"+")[1];
                auto f2 = cbu::string_split(f1,"-");
                string a = f2[0];
                string b = f2[1];

                status.ahead = stoi(a);
                status.behind = stoi(b);
            } else if (line.starts_with("#")) {

            } else {
                status.has_unsaved = true;
            }
        }

        if (!status.is_remote) {
            if (cbu::run_shell_command(dir,std::format("git branch -u origin/{} {}",status.branch,status.branch),&output)) {
                code = log_no_git();
            }
            if (code) return {.valid = false};
            continue;
        }

        break;
    }
    if (cbu::run_shell_command(dir,"git submodule status",&output)) {
        code = log_no_git();
        return {.valid = false};
    }
    for (string s : cbu::string_split(output,"\n")) {
        char c = s[0];
        cbu::log_verbose(std::format("Submodule entry {} first {}",s,c));
        vector<string> split = cbu::string_split(s.substr(1)," ");
        string commit = split[0];
        string path = split[1];

        status.submodules[path] = get_general_status(dir / path);
    }


    return status;
}

static U8 sync_dir(GitStatus status,fpath dir) {
    string output;
    U8 code = 0;
    if (status.ahead > 0 and status.behind > 0) {
        cbu::log_warn("Remote and local are out of sync!");

        code = cbu::run_shell_command(dir,std::format("git pull --rebase"),&output);
        if (code) {
            code = cbu::run_shell_command(dir,std::format("git rebase --abort"),&output);

            cbu::log_error(false,"Conflict detected!");
            return -1;
        }

        cbu::log_success("Automatic restore succesful");
    }

    if (status.ahead > 0) {
        cbu::log_info("Pushing local changes");

        if (cbu::run_shell_command(mcc::state::active_config->directory,std::format("git push"),&output)) {
            code = log_no_git();
        }

        if (code) return code;
    } else if (status.behind > 0) {
        cbu::log_info("Pulling changes from remote");

        if (cbu::run_shell_command(mcc::state::active_config->directory,std::format("git pull"),&output)) {
            code = log_no_git();
        }

        if (code) return code;
    } else {
        cbu::log_info("Everything is up to date");
    }

    return 0;
}
U8 mcc::git::sync() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code;

    GitStatus status;
    mcc::state::state_safe([&status]() {
        status = get_general_status(mcc::state::current_project);
    });
    if (!status.valid) return -1;
    if (status.has_unsaved) return log_unsaved();

    for (auto &[key,sub] : status.submodules) {
        mcc::state::state_safe([&code,status]() {
            code = sync_dir(status,mcc::state::current_project);
        });
        if (code) {
            cbu::log_error(false,"Failed to sync submodule");

            return code;
        }
    }
    mcc::state::state_safe([&code,status]() {
        code = sync_dir(status,mcc::state::current_project);
    });
    if (code) return code;

    return mcc::git::status();
}
U8 mcc::git::undo() {
    if (!mcc::config::valid()) return log_no_conf();

    GitStatus status;
    mcc::state::state_safe([&status]() {
        status = get_general_status(mcc::state::current_project);
    });
    if (!status.valid) return -1;

    if (!status.is_remote) {
        cbu::log_error(false,"Your branch is not connected to the remote!");

        return -1;
    }
    if (status.ahead <= 0) {
        bool res;
        cbu::cli_input("Your branch is not ahead of the remote, undoing now will go back in time by 1 commit. Proceed? (default false)");
        cbu::cli_get_valid_bool(&res,true);

        if (!res) return 0;
    }

    string output;
    U8 code = 0;
    mcc::state::state_safe([&output,&code]() {
        if (cbu::run_shell_command(mcc::state::active_config->directory,"git reset HEAD~1",&output)) {
            code = log_no_git();
        }
    });
    if (!code) return mcc::git::status();

    return code;
}
static void print_status_info(GitStatus status) {
    cbu::cli_output(std::format("Current branch: {}",status.branch));
    if (status.is_remote) {
        cbu::cli_output(std::format("{} Commit{} behind remote",status.behind,cbu::string_n("","s",status.behind)));
        cbu::cli_output(std::format("{} Commit{} ahead remote",status.ahead,cbu::string_n("","s",status.ahead)));
    } if (status.has_unsaved) {
        cbu::cli_output("Has unsaved changes.");
    }

    for (auto &[key,stat] : status.submodules) {
        cbu::cli_output(std::format("---\nSubmodule {}",key));
        print_status_info(stat);
    }
}
U8 mcc::git::status() {
    if (!mcc::config::valid()) return log_no_conf();

    GitStatus status;
    mcc::state::state_safe([&status]() {
        status = get_general_status(mcc::state::current_project);
    });
    if (!status.valid) return -1;

    print_status_info(status);

    return 0;
}

U8 mcc::git::commit_all(string msg) {
    if (!mcc::config::valid()) return log_no_conf();

    GitStatus status;
    mcc::state::state_safe([&status]() {
        status = get_general_status(mcc::state::current_project);
    });
    if (!status.valid) return -1;
    if (!status.has_unsaved) {
        cbu::log_warn("No local changes to commit!");
        return 0;
    }
    string output;
    U8 code = 0;
    mcc::state::state_safe([&output,&code,msg]() -> void {
        code = cbu::run_shell_command(mcc::state::current_project,"git add .",&output);
        if (code) { log_no_git(); return; }
        code = cbu::run_shell_command(mcc::state::current_project,std::format("git commit -m \"{}\"",msg),&output);
        if (code) log_no_git();
    });
    if (code) return code;

    cbu::log_success("Commit succesful");
    return mcc::git::status();
}
