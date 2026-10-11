#include "git_commands.hpp"
#include "ansi.hpp"
#include "base_types.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "state.hpp"
#include "stringmath.hpp"
#include <filesystem>
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
    bool is_attached = true;
    bool is_remote = false;
    bool has_unsaved = false;
    S64 ahead = 0;
    S64 behind = 0;

    vector<string> modified;
    vector<string> deleted;
    vector<string> untracked;
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
                string branch = line.substr(14);
                if (branch == "(detached)") status.is_attached = false;
                else status.branch = branch;
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

            } else if (line.starts_with("1")) {
                auto split = cbu::string_split(line," ");
                if (split.size() < 9) continue;

                if (split[2] == "S.M.") split[8] = std::format("Submodule: \"{}\"",split[8]);
                else status.has_unsaved = true;
                if (split[1] == ".M") { // modified
                    status.modified.push_back(split[8]);
                } else if (split[1] == ".D") {
                    status.deleted.push_back(split[8]);
                } else {
                    cbu::log_warn(std::format("Unrecognized git status {}!",split[1]));
                }
            } else if (line.starts_with("?")) {
                auto split = cbu::string_split(line," ");
                status.untracked.push_back(split[1]);
            }
        }

        if (!status.is_remote and status.is_attached) {
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
    for (auto &[key,sub] : status.submodules) {
        if (sync_dir(sub,dir / key)) {
            cbu::log_error(false,"Failed to sync submodule");

            return -1;
        }
    }

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

        if (cbu::run_shell_command(dir,std::format("git push"),&output)) {
            code = log_no_git();
        }

        if (code) return code;
    } else if (status.behind > 0) {
        cbu::log_info("Pulling changes from remote");

        if (cbu::run_shell_command(dir,std::format("git pull"),&output)) {
            code = log_no_git();
        }

        if (code) return code;
    } else {
        cbu::log_info("Everything is up to date");
    }

    return 0;
}

static U8 fix_dir(fpath dir) {
    string output;
    GitStatus status = get_general_status(dir);
    if (!status.valid) {
        cbu::log_error(false,"Status is invalid! Does a git repository exist?");
        return -1;
    }
    bool did_fix = false;
    for (auto &[key,sub] : status.submodules) {
        if (fix_dir(dir / key)) return -1;
    }
    if (!status.is_remote or !status.is_attached) {
        did_fix = true;
        cbu::log_warn("Git head is detached!");
        cbu::cli_input("Enter branch name to attach to");
        string branch;
        if (!cbu::cli_get_valid_string(&branch)) {
            cbu::log_error(false,"Branch cannot be empty");

            return -1;
        }

        if (cbu::run_shell_command(dir,std::format("git checkout {}",branch),&output)) {
            cbu::log_error(false,"Git checkout failed");

            return -1;
        }

        cbu::log_success("Succesfully attached head!");
    }
    if (!did_fix) cbu::log_success("No fixes are needed");

    return 0;
}
static void print_status_info(GitStatus status,bool full) {
    cbu::cli_output(std::format("Current branch: {}",status.branch));
    if (status.is_remote) {
        cbu::cli_output(std::format("{} Commit{} behind remote",status.behind,cbu::string_n("","s",status.behind)));
        cbu::cli_output(std::format("{} Commit{} ahead remote",status.ahead,cbu::string_n("","s",status.ahead)));
    } if (status.has_unsaved) {
        cbu::cli_output("Has unsaved changes.",cbu::ansi_color_orange);
    }

    if (full) {

        if (!status.modified.empty() or !status.deleted.empty() or !status.untracked.empty()) cbu::cli_output("---");
        for (auto &s : status.modified) {
            cbu::cli_output(std::format("Modified: {}",s));
        } if (!status.modified.empty()) cbu::cli_output("-");
        for (auto &s : status.deleted) {
            cbu::cli_output(std::format("Deleted: {}",s),cbu::ansi_color_red);
        } if (!status.deleted.empty()) cbu::cli_output("-");
        for (auto &s : status.untracked) {
            cbu::cli_output(std::format("Untracked: {}",s));
        }
    }

    for (auto &[key,stat] : status.submodules) {
        cbu::cli_output(std::format("---\nSubmodule {}",key));
        print_status_info(stat,full);
    }
}
static U8 print_status_full(fpath dir,bool full) {
    GitStatus status = get_general_status(dir);
    if (!status.valid) return -1;

    print_status_info(status,full);

    return 0;
}
static U8 discard_all(fpath dir) {
    string output;
    return cbu::run_shell_command(dir,"git reset --hard; git clean -fd",&output);
}
static U8 prompt_remove_unsaved(fpath dir) {
    string output;
    cbu::cli_input("Unsaved changes detected! Discard all unsaved changes? (default false)");
    bool res;
    cbu::cli_get_valid_bool(&res,true);
    if (!res) return -1;

    return discard_all(dir);
}
static U8 checkout_branch(fpath dir,string target) {
    string output;
    GitStatus status = get_general_status(dir);

    if (!status.valid) return -1;
    if (status.has_unsaved and prompt_remove_unsaved(dir)) return -1;

    if (sync_dir(status,dir)) {
        cbu::log_error(false,"Sync failed, aborting");

        return -1;
    }
    if (cbu::run_shell_command(dir,std::format("git checkout {}",target),&output)) {
        cbu::log_error(false,"Git checkout failed");

        return -1;
    }

    return 0;
}

U8 mcc::git::sync() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code]() {
        fpath dir = mcc::state::current_project;

        GitStatus status = get_general_status(dir);
        if (!status.valid) { code = -1; return; }
        if (status.has_unsaved and prompt_remove_unsaved(dir)) { code = -1; return; }

        code = sync_dir(status,mcc::state::current_project);
        if (code) return;

        print_status_full(dir,false);
    });

    return code;
}

static U8 undo_dir(fpath dir) {
    GitStatus status = get_general_status(dir);

    if (!status.valid) return -1;
    string output;

    if (!status.is_remote) {
        cbu::log_error(false,"Your branch is not connected to the remote!");

        return -1;
    }
    if (status.ahead <= 0) {
        bool res;
        cbu::cli_input("Your branch is not ahead of the remote, undoing now will go back in time by 1 commit. Proceed? (default false)");
        cbu::cli_get_valid_bool(&res,true);
        if (!res) return 1;
    }
    U8 code = cbu::run_shell_command(mcc::state::active_config->directory,"git reset HEAD~1",&output);
    if (code) { log_no_git(); return code; }

    print_status_full(dir,true);
    return 0;
}
U8 mcc::git::undo() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code]() {
        code = undo_dir(mcc::state::current_project);
    });

    return code;
}
U8 mcc::git::subundo(string sub) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code,sub]() {
        code = undo_dir(mcc::state::current_project / sub);
    });

    return code;
}
U8 mcc::git::status() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code]() {
        code = print_status_full(mcc::state::current_project,true);
    });

    return code;
}

U8 mcc::git::commit_all(string msg) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code,msg]() {
        fpath dir = mcc::state::current_project;
        string output;

        GitStatus status = get_general_status(dir);

        if (!status.valid) { code = -1; return; }
        if (!status.has_unsaved) {
            cbu::log_success("No local changes to commit!");
            return;
        }

        code = cbu::run_shell_command(dir,"git add .",&output);
        if (code) { log_no_git(); return; }
        code = cbu::run_shell_command(dir,std::format("git commit -m \"{}\"",msg),&output);
        if (code) { log_no_git(); return; }

        cbu::log_success("Commit succesful");
    });


    return code;
}
U8 mcc::git::commit_all_sub(string sub,string msg) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code,msg,sub]() {
        fpath dir = mcc::state::current_project / sub;
        string output;

        GitStatus status = get_general_status(dir);

        if (!status.valid) { code = -1; return; }
        if (!status.has_unsaved) {
            cbu::log_success("No local changes to commit!");
            return;
        }

        code = cbu::run_shell_command(dir,"git add .",&output);
        if (code) { log_no_git(); return; }
        code = cbu::run_shell_command(dir,std::format("git commit -m \"{}\"",msg),&output);
        if (code) { log_no_git(); return; }

        cbu::log_success("Commit succesful");
    });

    return code;
}
U8 mcc::git::change_branch(string target) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code;
    mcc::state::state_safe([&code,target]() {
        fpath dir = mcc::state::current_project;
        code = checkout_branch(dir,target);
        if (code) return;

        GitStatus status = get_general_status(dir);
        if (!status.valid) {
            cbu::log_error(false,"Failed to get status after checkout");

            code = -1;
            return;
        }
        cbu::log_success("Succesfully switched branches");
        code = sync_dir(status,dir);
        if (code) return;

        code = print_status_full(dir,false);
    });

    return code;
}
U8 mcc::git::new_branch(string from,string name) {
    U8 code = 0;

    mcc::state::state_safe([&code,&name]() {
        fpath dir = mcc::state::current_project;

        if (checkout_branch(dir,name)) {
            cbu::log_error(false,"Failed to checkout to from, maybe it doesn't exist?");

            code = -1;
            return;
        }
        string output;

        if (cbu::run_shell_command(dir,std::format("git checkout -b {}",name),&output)) {
            cbu::log_error(false,"Failed to create new branch");

            code = -1;
            return;
        }
        if (cbu::run_shell_command(dir,std::format("git push -u origin {}",name),&output)) {
            cbu::log_error(false,"Failed to push new branch");

            code = -1;
            return;
        }

        code = checkout_branch(dir,name);
    });

    return code;
}
U8 mcc::git::merge(string base,string feature) {
    cbu::log_error(false,"Not implemented");

    return -1;
}
U8 mcc::git::fix() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code;
    mcc::state::state_safe([&code]() {
        code = fix_dir(mcc::state::current_project);
    });

    return code;
}
U8 mcc::git::clone(string target,string url) {
    fpath dir = target;

    std::filesystem::create_directories(dir.parent_path());
    if (cbu::run_shell_command(dir.parent_path(),std::format("git clone --recurse-submodules {} {}",url,cbu::path_to_utf8(dir)),NULL)) {
        cbu::log_error(false,"Failed to clone repository");

        return -1;
    }
    if (fix_dir(dir)) {
        cbu::log_error(false,"Failed to fix repository");

        return -1;
    }

    cbu::log_success("Cloning succesful");
    GitStatus status = get_general_status(dir);
    if (!status.valid) return -1;
    print_status_info(status,true);

    return 0;
}
