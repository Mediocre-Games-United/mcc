// proceed with caution, ugly code warning

#include "base_types.hpp"
#include "config_file.hpp"
#include "cli.hpp"
#include "commands.hpp"
#include "compiler.hpp"
#include "config_commands_generic.hpp"
#include "config_commands_special.hpp"
#include "config_file_format.hpp"
#include "file.hpp"
#include "installer.hpp"
#include "logger.hpp"
#include "binary.hpp"
#include "lsp_file.hpp"
#include "shell.hpp"
#include "state.hpp"
#include <cstdint>
#include <filesystem>
#include <format>
#include <queue>
#include "vectormath.hpp"

using namespace mcc::config;

ConfigContainer *mcc::config::current_config = NULL;

using cf = ConfigObject;
static void scan_config();


class ConfigContainerFileFormatV1 : public cbu::BinaryFileVersion {
public:
    cbu::BinaryFileSection *get_sections() override {
        return new cbu::MainBinaryFileSection({
            new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* {
                auto *obj = (ConfigContainer*) u;
                *len = obj->loaded_configs.size();
                *size = sizeof(ConfigContainerDataBlock);

                ConfigContainerDataBlock *p = new ConfigContainerDataBlock[*len];
                for (size_t i = 0; i < obj->loaded_configs.size(); i ++) {
                    auto &cfg = obj->loaded_configs[i];
                    p[i].obj = cfg;
                    p[i].fpath = cbu::path_to_utf8(cfg->directory / string(FNAME));
                }

                return p;
            },[](void *obj) { delete[] (ConfigContainerDataBlock*) obj; },{
                new cbu::DataBinarySection([](void *u,void *d) { // config file path
                    auto *ct = (ConfigContainer*) u;
                    ct->pending_datablocks.push(new ConfigContainerDataBlock(*((ConfigContainerDataBlock*) d)));
                },[]() -> void* { return new ConfigContainerDataBlock(); },[](void *obj) { delete (ConfigContainerDataBlock*) obj; },{
                    new cbu::StringBinarySection([](void *obj,auto value) {
                        auto *db = (ConfigContainerDataBlock*) obj;
                        db->fpath = value;
                    },[](void *obj) -> string {
                        return ((ConfigContainerDataBlock*) obj)->fpath;
                    })
                })
            })
        });
    }
};

class ConfigContainerFileFormat : public cbu::BinaryFileFormat {
public:
    ConfigContainerFileFormat() : cbu::BinaryFileFormat({new ConfigContainerFileFormatV1()},"cfg_ctr") {};
    void load_configs() {
        ConfigContainer *ct = new ConfigContainer();
        load_file_to_buffer(cbu::resolve_path("user://configs.cfg_ctr"));
        load_buffer_to_object(ct);
        size_t config_count = 0;

        current_config = ct;
        auto fmt = ConfigFileFormat();
        while (!ct->pending_datablocks.empty()) {
            auto ref = ct->pending_datablocks.front();

            cbu::log_verbose(std::format("Loading config id {} at {}",config_count,ref->fpath));
            config_count += 1;

            if (std::filesystem::exists(ref->fpath)) {
                cf *cfg = fmt.load_config(ref->fpath);
                auto ex = find_config_by_name(cfg->name);
                if (ex) {
                    cbu::log_verbose("Config exists!");
                    delete cfg;

                    continue;
                }

                update_config(cfg);
                link_config(cfg);
            }

            delete ref;
            ct->pending_datablocks.pop();
        }
        cbu::log_debug(std::format("Loaded {} config files",config_count));
    }
    void save_configs() {
        load_object_to_buffer(current_config);
        save_buffer_to_file(cbu::resolve_path("user://configs.cfg_ctr"));
    }
};


static void scan_config() {
    cbu::log_verbose("Scanning local config file");
    auto fmt = ConfigContainerFileFormat();
    if (current_config) delete current_config;
    mcc::state::state_safe([]() {
        mcc::state::active_config = NULL;
    });

    fmt.load_configs();
}

bool mcc::config::link_config(ConfigObject *obj) {
    if (!current_config) return false;

    bool exists = false;
    for (auto &s : current_config->loaded_configs) {
        if (s->directory == obj->directory) {
            exists = true;
            break;
        }
    }
    if (exists) {
        cbu::log_debug("Config is already linked! Skipping...");
        return true;
    }

    current_config->loaded_configs.push_back(obj);
    auto fmt = ConfigContainerFileFormat();
    fmt.save_configs();
    return true;
}

static bool subprojects_have_src_file(SourceFileObject *&src,vector<cf*> &subconfigs) {
    for (auto &s : subconfigs) {
        for (auto &sr : s->source_files) {
            if (sr->name != src->name) continue;

            cbu::log_verbose(std::format("Subproject {} src {} matches with t {}",s->name,sr->name,src->name));
            return true;
        }
    }

    return false;
}
static void config_recurse_src_files(vector<cf*> &subconfigs,SourceFileObject *&main,vector<SourceFileObject*> &sourcefiles,fpath root,fpath path,bool parent_no_match = true) {
    bool no_match = parent_no_match;
    if (no_match) {
        for (auto &s : subconfigs) {
            if (path == s->directory) {
                no_match = false;
                break;
            }
        }
    }

    for (auto &s : cbu::iterate_dir(path)) {
        if (std::filesystem::is_directory(s)) {
            string name = cbu::path_to_utf8(s.filename());
            if (name == ".mcc" || name == ".git") continue;

            config_recurse_src_files(subconfigs,main,sourcefiles,root,s,no_match);
            continue;
        }
        if (!std::filesystem::is_regular_file(s)) continue;
        string ext = s.extension().string();
        if (ext == ".cpp" || ext == ".c") {
            string name = cbu::path_to_utf8(s.stem());
            fpath p = std::filesystem::relative(s,root);

            // cbu::log_debug(std::format("Found source file {} with name {}",cbu::path_to_utf8(p),name));
            bool file_exists = false;
            for (auto &sr : sourcefiles) {
                if (sr->path == p) {
                    file_exists = true;
                    break;
                }
            }
            if (file_exists) {
                // cbu::log_debug("File already exists! Skipping...");
                continue;
            }
            if (name == "main") {
                if (main) continue;

                cbu::log_verbose(std::format("Found main at {}",cbu::path_to_utf8(s)));
                main = new SourceFileObject{
                    .path = p,
                    .name = name
                };
                continue;
            }
            if (!no_match) continue;
            sourcefiles.push_back(new SourceFileObject{
                .path = p,
                .name = name
            });

            continue;
        } if (ext == ".hpp" || ext == ".h") {
            // cbu::log_debug(std::format("Found header file {}",cbu::path_to_utf8(s)));
            continue;
        }
    }
}
static void config_recurse_sub_projects(vector<cf*> &subconfigs,fpath path) {
    for (auto &s : cbu::iterate_dir(path)) {
        if (std::filesystem::is_directory(s)) {
            config_recurse_sub_projects(subconfigs,s);
            continue;
        }
        if (!std::filesystem::is_regular_file(s)) continue;
        if (s.filename() != FNAME) continue;

        cbu::log_debug(std::format("Found subconfig {} at {}",FNAME,cbu::path_to_utf8(s)));
        auto fmt = ConfigFileFormat();
        cf *cfg = fmt.load_config(s);

        bool found = false;
        for (auto &o : subconfigs) {
            if (o->name != cfg->name || o->directory != cfg->directory) continue;

            cbu::log_verbose("Subconfig already exists! Skipping...");
            found = true;
            delete cfg;
            break;
        }

        if (found) continue;

        auto ex = find_config_by_path(cfg->directory);
        if (ex) {
            cbu::log_verbose("Subconfig is already linked! Skipping...");
            delete cfg;

            cfg = ex;
        }
        update_config(cfg);

        link_config(cfg);
        subconfigs.push_back(cfg);

        continue;
    }
}
void mcc::config::update_config(cf *obj) {
    vector<cf*> subconfigs = obj->sub_projects;
    vector<SourceFileObject*> sourcefiles = obj->source_files;

    SourceFileObject *main = NULL;
    auto lpath = obj->directory / obj->src_directory;
    config_recurse_sub_projects(subconfigs,lpath);
    config_recurse_src_files(subconfigs,main,sourcefiles,lpath,lpath);

    vector<SourceFileObject*> remove_queue{};
    for (auto &s : subconfigs) update_config(s);
    for (auto &s : sourcefiles) {
        if (std::filesystem::exists(lpath / s->path) and !subprojects_have_src_file(s,subconfigs)) continue;
        remove_queue.push_back(s);
    }
    for (auto &s : remove_queue) {
        cbu::vector_erase_value(sourcefiles,s);
        delete s;
    }

    obj->source_files = sourcefiles;
    obj->sub_projects = subconfigs;

    obj->main_source = main;
    auto fmt = ConfigFileFormat();
    fmt.save_config(obj);

    mcc::lsp::generate_all_lsps_for_config(obj);
}

void mcc::config::init() {
    scan_config();
}
uint8_t mcc::config::reload() {
    scan_config();

    return 0;
}
void mcc::config::background() {
    mcc::state::state_safe([]() {
        if (!mcc::state::active_config) return;

        update_config(mcc::state::active_config);
        auto fmt = ConfigFileFormat();
        fmt.save_config(mcc::state::active_config);
    });
}


uint8_t mcc::config::cmd() {
    cbu::cli_output("Welcome to the config utility!");
    string cmd;
    while (true) {
        if (mcc::consume_interrupt()) return -1;

        cbu::cli_input("CONFIG: Enter command (h for help)");
        cmd = cbu::cli_get_string();

        bool has_new_project_prepath = false;
        fpath new_project_prepath;
        if (cmd == "h") {
            cbu::cli_output("[l] list configs\n[n] new config\n[e] edit existing\n[a] link existing config\n[q] quit config utility\n[s] set config as the active one for other commands\n[x] add external library to config\n[p] add parent directory such as for a subproject dependent on a parent config.h file\n[t] add export target to config");
            continue;
        } if (cmd == "q") {
            cbu::cli_output("Exiting config utility...");
            break;
        } if (cmd == "l") {
            mcc::config_commands::list_configs_cmd();

            continue;
        } if (cmd == "a") {
            mcc::config_commands::link_config_cmd();

            continue;
        } if (cmd == "p") {
            mcc::config_commands::config_parent_dir_cmd();

            continue;
        }  if (cmd == "t") {
            mcc::config_commands::config_add_export_cmd();

            continue;
        } if (cmd == "x") {
            mcc::config_commands::config_add_external_cmd();

            continue;
        } if (cmd == "n") {
            mcc::config_commands::new_config_cmd();

            continue;
        } if (cmd == "s") {
            if (!current_config) continue;
            cbu::cli_input("Enter config name to set current");
            string name = cbu::cli_get_string();

            set_name_current(name);

            continue;
        } if (cmd == "e") {
            mcc::config_commands::edit_config_cmd();

            continue;
        }

        cbu::log_warn("Unknown config command");
    }

    return 0;
}
bool mcc::config::valid() {
    bool v;
    mcc::state::state_safe([&v]() {
        v = mcc::state::active_config;
    });

    return v;
}


static string last_config_name;
bool mcc::config::link_config_by_file(fpath path) {
    if (!std::filesystem::exists(path)) {
        cbu::log_error(false,"File does not exist");
        return false;
    } if (std::filesystem::is_directory(path)) {
        path = path / FNAME;
    } if (!std::filesystem::is_regular_file(path)) {
        cbu::log_error(false,"Path is not a file!");
        return false;
    } if (path.filename() != "project.mcc") {
        cbu::log_error(false,"According to our arbitrary restrictions, the file has to be called project.mcc to work :nerd:");
        return false;
    }

    auto fmt = ConfigFileFormat();
    cf *cfg = fmt.load_config(path);
    update_config(cfg);
    link_config(cfg);
    last_config_name = cfg->name;

    cbu::log_success(std::format("Found & loaded config {} succesfully",cfg->name));
    return true;
}
bool mcc::config::set_file_current(fpath ppath) {
    bool s = link_config_by_file(ppath);
    if (!s) return false;

    return set_name_current(last_config_name);
}
bool mcc::config::set_name_current(string name) {
    auto obj = find_config_by_name(name);
    if (!obj) {
        cbu::log_warn(std::format("Project {} does not exist or is unlinked",name));
        return false;
    }


    mcc::state::state_safe([obj]() {
        mcc::state::active_config = obj;
        mcc::state::current_project = obj->directory;
    });

    cbu::log_success(std::format("Project {} at {} is now current",name,cbu::path_to_utf8(obj->directory)));
    return true;
}
ConfigObject *mcc::config::find_config_by_name(string name) {
    for (auto &s : current_config->loaded_configs) {
        if (s->name != name) continue;

        return s;
    }
    return NULL;
}
ConfigObject *mcc::config::find_config_by_path(fpath path) {
    for (auto &s : current_config->loaded_configs) {
        if (s->directory != path) continue;

        return s;
    }
    return NULL;
}
