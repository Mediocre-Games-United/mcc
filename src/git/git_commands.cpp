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
#include "workers.hpp"
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
static U8 log_unsaved() {
    cbu::log_error(false,"Commit unsaved changes first!");

    return -1;
}

struct GitStatus {
    bool valid = true;
    fpath dir;

    umap<string,GitStatus> submodules;
    string branch = "<detached>";
    bool is_attached = false;
    bool is_remote = false;
    bool has_unsaved = false;
    S64 ahead = 0;
    S64 behind = 0;

    vector<string> modified;
    vector<string> deleted;
    vector<string> untracked;
};
static void status_get_all_dirs(GitStatus status,vector<fpath> &list) {
    list.push_back(status.dir);
    for (auto &[path,sub] : status.submodules) {
        status_get_all_dirs(sub,list);
    }
}
static U8 read_current_git_status(fpath dir,GitStatus &status) {
    status.dir = dir;
    string output;

    if (cbu::run_shell_command(dir,"git status --porcelain=v2 --branch -z",&output)) {
        status.valid = false;
        cbu::log_error(false,"git status failed! Maybe a git repository doesn't exist?");
        return -1;
    }

    status.modified.clear();
    status.untracked.clear();
    status.deleted.clear();
    status.is_remote = false;
    status.has_unsaved = false;

    for (string line : separate_lines(output)) {
        cbu::log_debug(line);

        if (line.starts_with("# branch.head ")) {
            string branch = line.substr(14);
            if (branch == "(detached)") {
                status.branch = "<detached>";
                status.is_attached = false;
            } else {
                status.branch = branch;
                status.is_attached = true;
            }
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
            if (split[1][0] == 'M') { // modified, staged
                status.modified.push_back(split[8]);
            } else if (split[1][1] == 'M') { // modified, unstaged
                status.modified.push_back(split[8]);
            } else if (split[1][0] == 'D') { // deleted, staged
                status.deleted.push_back(split[8]);
            } else if (split[1][1] == 'D') { // deleted, unstaged
                status.deleted.push_back(split[8]);
            } else if (split[1][0] == 'A') { // new, staged
                status.modified.push_back(split[8]);
            } else {
                cbu::log_warn(std::format("Unrecognized git status {}!",split[1]));
            }
        } else if (line.starts_with("?")) {
            auto split = cbu::string_split(line," ");
            status.untracked.push_back(split[1]);
        }
    }

    if (cbu::run_shell_command(dir,"git submodule status",&output)) {
        status.valid = false;
        cbu::log_error(false,"git submodule status failed! Maybe a git repository doesn't exist?");

        return -1;
    }
    for (string s : cbu::string_split(output,"\n")) {
        char c = s[0];
        cbu::log_verbose(std::format("Submodule entry {} first {}",s,c));
        vector<string> split = cbu::string_split(s.substr(1)," ");
        string commit = split[0];
        string path = split[1];

        GitStatus substatus{};
        if (read_current_git_status(dir / path,substatus)) {
            cbu::log_error(false,"Failed to read git status for submodule");

            status.valid = false;
            return -1;
        }
        status.submodules[path] = substatus;
    }

    return 0;
}
static GitStatus get_general_status(fpath dir,bool get_network) {
    GitStatus status{};

    string output;
    U8 code = read_current_git_status(dir,status);
    if (code) return status;

    if (get_network) {
        cbu::cli_output("Fetching git status from remote...");
        vector<fpath> dirs{};
        status_get_all_dirs(status,dirs);

        vector<std::function<void()>> calls{};

        std::atomic<bool> succesful = true;
        for (auto &d : dirs) {
            calls.push_back([d,&succesful]() {
                string output;
                U8 code = cbu::run_shell_command(d,"git fetch",&output);
                if (!code) return;

                cbu::log_error(false,"git fetch failed!");
                succesful = false;
            });
        }
        cbu::run_tasks_sync(calls);
        if (!succesful) return {.valid = false};
        if (read_current_git_status(dir,status)) return status;

        if (!status.is_attached) {
            cbu::cli_input("Git is not attached to a branch! Enter branch name to attach to");
            string br;
            if (!cbu::cli_get_valid_string(&br)) {
                cbu::log_error(false,"Branch cannot be empty");
                return {.valid = false};
            }
            if (cbu::run_shell_command(dir,std::format("git checkout {}",br),&output)) {
                cbu::log_error(false,"git checkout failed! Invalid branch name?");
                return {.valid = false};
            }

            if (read_current_git_status(dir,status)) return status;
        }
        if (!status.is_remote and status.is_attached) {
            if (cbu::run_shell_command(dir,std::format("git branch -u origin/{} {}",status.branch,status.branch),&output)) {
                return {.valid = false};
            }
            if (read_current_git_status(dir,status)) return status;
        }
    }

    return status;
}

static U8 sync_dir(GitStatus status,fpath dir) {
    vector<fpath> dirs{};
    status_get_all_dirs(status,dirs);

    std::atomic<bool> succesful = true;
    std::mutex read_mutex{};
    vector<std::function<void()>> work{};
    for (auto &d : dirs) {
        work.push_back([d,&succesful,&read_mutex]() {
            string output;
            U8 code = 0;
            GitStatus status{};

            read_mutex.lock();
            code = read_current_git_status(d,status);
            read_mutex.unlock();
            if (code) {
                succesful = false;
                return;
            }

            if (status.ahead > 0 and status.behind > 0) {
                cbu::log_warn("Remote and local are out of sync!");

                code = cbu::run_shell_command(d,std::format("git pull --rebase"),&output);
                if (code) {
                    code = cbu::run_shell_command(d,std::format("git rebase --abort"),&output);

                    cbu::log_error(false,"Conflict detected!");
                    succesful = false;
                    return;
                }

                cbu::log_success("Automatic rebase succesful with no conflicts detected!");
                status.behind = 0;
            }
            if (status.ahead > 0) {
                cbu::log_info("Pushing local changes");

                if (cbu::run_shell_command(d,std::format("git push"),&output)) {
                    cbu::log_error(false,"git push failed!");

                    succesful = false;
                    return;
                }

            } else if (status.behind > 0) {
                cbu::log_info("Pulling changes from remote");

                if (cbu::run_shell_command(d,std::format("git pull"),&output)) {
                    cbu::log_error(false,"git pull failed!");

                    succesful = false;
                    return;
                }

            }
        });
    }

    cbu::run_tasks_sync(work);
    if (!succesful) return -1;

    return 0;
}

static U8 fix_dir(fpath dir) {
    string output;
    GitStatus status = get_general_status(dir,true);
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
    string label = std::format("Current branch: {}",status.branch);
    if (status.has_unsaved) {
        label = std::format("{}, {}",label,cbu::ansi_wrap_string("Has unsaved changes.",cbu::ansi_color_orange));
    }

    cbu::cli_output(label);
    if (status.is_remote) {
        string ahead_label = "";
        string behind_label = "";

        if (status.behind <= 0) {
            behind_label = cbu::ansi_wrap_string("0 Commits behind",cbu::ansi_color_green);
        } else {
            behind_label = cbu::ansi_wrap_string(std::format("{} Commit{} behind",status.behind,cbu::string_n("","s",status.behind)),cbu::ansi_color_yellow);
        }
        if (status.ahead <= 0) {
            ahead_label = cbu::ansi_wrap_string("0 Commits ahead",cbu::ansi_color_green);
        } else {
            ahead_label = cbu::ansi_wrap_string(std::format("{} Commit{} ahead",status.ahead,cbu::string_n("","s",status.ahead)),cbu::ansi_color_yellow);
        }

        cbu::cli_output(std::format("Remote: {}, {}",behind_label,ahead_label));
    }

    if (full) {

        if (!status.modified.empty() or !status.deleted.empty() or !status.untracked.empty()) cbu::cli_output("---");
        for (auto &s : status.modified) {
            cbu::cli_output(std::format("{}: {}",cbu::ansi_wrap_string("Modified",cbu::ansi_color_orange),s));
        } if (!status.modified.empty() and (!status.deleted.empty() or !status.untracked.empty())) cbu::cli_output("-");
        for (auto &s : status.deleted) {
            cbu::cli_output(std::format("{}: {}",cbu::ansi_wrap_string("Deleted",cbu::ansi_color_red),s));
        } if (!status.deleted.empty() and !status.untracked.empty()) cbu::cli_output("-");
        for (auto &s : status.untracked) {
            cbu::cli_output(std::format("{}: {}",cbu::ansi_wrap_string("Untracked",cbu::ansi_color_lime),s));
        }
    }

    for (auto &[key,stat] : status.submodules) {
        cbu::cli_output(std::format("---\nSubmodule {}",key));
        print_status_info(stat,full);
    }
}
static U8 print_status_full(fpath dir,bool full) {
    GitStatus status = get_general_status(dir,true);
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
    GitStatus status = get_general_status(dir,true);

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

        GitStatus status = get_general_status(dir,true);
        if (!status.valid) { code = -1; return; }
        if (status.has_unsaved and prompt_remove_unsaved(dir)) { code = -1; return; }

        code = sync_dir(status,mcc::state::current_project);
        if (code) return;

        print_status_full(dir,false);
    });
    mcc::config::background();

    return code;
}

static U8 undo_dir(fpath dir) {
    GitStatus status = get_general_status(dir,true);

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
    if (code) return code;

    print_status_full(dir,true);
    return 0;
}
U8 mcc::git::undo() {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code]() {
        code = undo_dir(mcc::state::current_project);
    });
    mcc::config::background();

    return code;
}
U8 mcc::git::subundo(string sub) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code,sub]() {
        code = undo_dir(mcc::state::current_project / sub);
    });
    mcc::config::background();

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

        GitStatus status = get_general_status(dir,false);

        if (!status.valid) { code = -1; return; }
        if (!status.has_unsaved) {
            cbu::log_success("No local changes to commit!");
            return;
        }

        code = cbu::run_shell_command(dir,"git add .",&output);
        if (code) return;
        code = cbu::run_shell_command(dir,std::format("git commit -m \"{}\"",msg),&output);
        if (code) return;

        cbu::log_success("Commit succesful");
        status = get_general_status(dir,false);
        print_status_info(status,true);
    });


    return code;
}
U8 mcc::git::commit_all_sub(string sub,string msg) {
    if (!mcc::config::valid()) return log_no_conf();

    U8 code = 0;
    mcc::state::state_safe([&code,msg,sub]() {
        fpath dir = mcc::state::current_project / sub;
        string output;

        GitStatus status = get_general_status(dir,false);

        if (!status.valid) { code = -1; return; }
        if (!status.has_unsaved) {
            cbu::log_success("No local changes to commit!");
            return;
        }

        code = cbu::run_shell_command(dir,"git add .",&output);
        if (code) return;
        code = cbu::run_shell_command(dir,std::format("git commit -m \"{}\"",msg),&output);
        if (code) return;

        cbu::log_success("Commit succesful");
        status = get_general_status(dir,false);
        print_status_info(status,true);
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

        GitStatus status = get_general_status(dir,true);
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
    mcc::config::background();

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
    mcc::config::background();

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
    mcc::config::background();

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
    GitStatus status = get_general_status(dir,true);
    if (!status.valid) return -1;
    print_status_info(status,true);

    return 0;
}

static umap<string,string> get_stashes_names(fpath dir) {
    string output;
    if (cbu::run_shell_command(dir,"git stash list",&output)) {
        return {};
    }
    umap<string,string> list{};
    vector<string> split = cbu::string_split(output,"\n");
    for (auto &s : split) {
        cbu::log_debug(s);
    }

    return list;
}
static void remove_stashes_with_name(fpath dir,string name) {
    auto list = get_stashes_names(dir);
}
U8 mcc::git::safe_clean() {
    U8 code = 0;
    mcc::state::state_safe([&code]() {
        fpath dir = mcc::state::current_project;

        string output;
        remove_stashes_with_name(dir,"mcc:clean_autosave");
        // if (cbu::run_shell_command(dir,"git stash -m \"mcc:clean_autosave\"",&output)) { code = -1; return; }
    });

    return code;
}
U8 mcc::git::stash_all() {
    cbu::log_error(true,"Not implemented");

    return 0;
}
U8 mcc::git::restore_stashes() {
    cbu::log_error(true,"Not implemented");

    return 0;
}
