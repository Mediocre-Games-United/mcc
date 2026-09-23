// proceed with caution, ugly code warning

#include "base_types.hpp"
#include "config_file.hpp"
#include "cli.hpp"
#include "compiler.hpp"
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



struct ConfigContainerDataBlock {
    string fpath;
    mcc::config::ConfigObject *obj = NULL;
};
struct ConfigContainer {
    ~ConfigContainer() { for (auto &s : loaded_configs) delete s; }
    std::vector<mcc::config::ConfigObject*> loaded_configs;
    std::queue<ConfigContainerDataBlock*> pending_datablocks;
};


using cf = mcc::config::ConfigObject;
static ConfigContainer *current_config = NULL;
static bool link_config(mcc::config::ConfigObject *obj);
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
                    p[i].fpath = cbu::path_to_utf8(cfg->directory / string(mcc::config::FNAME));
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

static bool link_config(mcc::config::ConfigObject *obj) {
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

static void config_recurse_src_files(vector<cf*> &subconfigs,mcc::config::SourceFileObject *&main,vector<mcc::config::SourceFileObject*> &sourcefiles,fpath root,fpath path,bool parent_no_match = true) {
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
            config_recurse_src_files(subconfigs,main,sourcefiles,root,s,no_match);
            continue;
        }
        if (!std::filesystem::is_regular_file(s)) continue;
        string ext = s.extension().string();
        if (ext == ".cpp" || ext == ".c") {
            string name = cbu::path_to_utf8(s.stem());
            fpath p = std::filesystem::relative(s,root);

            cbu::log_debug(std::format("Found source file {} with name {}",cbu::path_to_utf8(p),name));
            bool file_exists = false;
            for (auto &sr : sourcefiles) {
                if (sr->path == p) {
                    file_exists = true;
                    break;
                }
            }
            if (file_exists) {
                cbu::log_debug("File already exists! Skipping...");
                continue;
            }
            if (name == "main") {
                if (main) continue;

                cbu::log_verbose(std::format("Found main at {}",cbu::path_to_utf8(s)));
                main = new mcc::config::SourceFileObject{
                    .path = p,
                    .name = name
                };
                continue;
            }
            if (!no_match) continue;
            sourcefiles.push_back(new mcc::config::SourceFileObject{
                .path = p,
                .name = cbu::path_to_utf8(s.parent_path() / name)
            });

            continue;
        } if (ext == ".hpp" || ext == ".h") {
            cbu::log_debug(std::format("Found header file {}",cbu::path_to_utf8(s)));
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
        if (s.filename() != mcc::config::FNAME) continue;

        cbu::log_debug(std::format("Found subconfig {} at {}",mcc::config::FNAME,cbu::path_to_utf8(s)));
        auto fmt = ConfigFileFormat();
        cf *cfg = fmt.load_config(s);

        bool found = false;
        for (auto &o : subconfigs) {
            if (o->name != cfg->name || o->directory != cfg->directory) continue;

            cbu::log_debug("Subconfig already exists! Skipping...");
            found = true;
            delete cfg;
            break;
        }

        if (found) continue;
        update_config(cfg);

        link_config(cfg);
        subconfigs.push_back(cfg);

        continue;
    }
}
void mcc::config::update_config(cf *obj) {
    vector<cf*> subconfigs = obj->sub_projects;
    vector<mcc::config::SourceFileObject*> sourcefiles = obj->source_files;

    mcc::config::SourceFileObject *main = NULL;
    auto lpath = obj->directory / obj->src_directory;
    config_recurse_sub_projects(subconfigs,lpath);
    config_recurse_src_files(subconfigs,main,sourcefiles,lpath,lpath);

    vector<mcc::config::SourceFileObject*> remove_queue{};
    for (auto &s : sourcefiles) {
        if (std::filesystem::exists(lpath / s->path)) continue;
        remove_queue.push_back(s);
    }
    for (auto &s : subconfigs) update_config(s);
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
            if (!current_config or current_config->loaded_configs.empty()) {
                cbu::log_warn("No configs found! Add some with a or create a new one with n");
                continue;
            }
            for (auto &s : current_config->loaded_configs) {
                cbu::cli_output(std::format("---\n{} at {}",s->name,cbu::path_to_utf8(s->directory)));
            }

            continue;
        } if (cmd == "a") {
            fpath path;
            cbu::cli_input("Enter the project path to link");
            if (!cbu::cli_get_valid_dirpath(&path)) {
                cbu::log_error(false,"Cancelled");
                continue;
            }

            fpath ppath = path / mcc::config::FNAME;
            if (!std::filesystem::exists(ppath)) {
                cbu::log_warn("Project has no config file!");
                cbu::cli_input("Create a new one?");
                bool res;
                bool valid = cbu::cli_get_valid_bool(&res);
                if (!valid || !res) continue;

                cmd = "n";
                has_new_project_prepath = true;
                new_project_prepath = path;
            } else {
                if (!std::filesystem::is_regular_file(ppath)) {
                    cbu::log_warn("Project is not a file");
                    continue;
                }
                link_file(ppath);

                continue;
            }
        } if (cmd == "p") {
            string name;
            fpath parentpath;

            cbu::cli_input("Enter config name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            bool match = false;
            cf *cfg;
            for (auto &s : current_config->loaded_configs) {
                if (s->name == name) {
                    match = true;
                    cfg = s;
                    break;
                }
            }
            if (!match) {
                cbu::log_warn("Config does not exist or is not linked!");
                continue;
            }
            cbu::cli_input("Enter parent path");
            if (!cbu::cli_get_valid_dirpath(&parentpath,cfg->directory)) {
                cbu::log_error(false,"Cancelled");
                continue;
            }

            fpath relpath = std::filesystem::relative(parentpath,cfg->directory);
            cbu::log_debug(std::format("Relative path: {}",cbu::path_to_utf8(relpath)));

            cfg->has_parent_directory = true;
            cfg->parent_directory = relpath;

            auto fmt = ConfigFileFormat();
            fmt.save_config(cfg);

            cbu::log_success("Added parent directory succesfully!");
            continue;
        }  if (cmd == "t") {
            string name;
            fpath parentpath;

            cbu::cli_input("Enter config name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            bool match = false;
            cf *cfg;
            for (auto &s : current_config->loaded_configs) {
                if (s->name == name) {
                    match = true;
                    cfg = s;
                    break;
                }
            }
            if (!match) {
                cbu::log_warn("Config does not exist or is not linked!");
                continue;
            }
            cbu::cli_input("Enter export type\nDefault (0 default)");
            int opt;
            if (!cbu::cli_get_valid_int(&opt,0,0,true,0)) {
                cbu::log_error(false,"Invalid value");
                continue;
            }
            mcc::config::ExportType exp;
            switch (opt) {
                case 0: {
                    exp = mcc::config::ExportType::EXPORT_DEFAULT;
                    break;
                }
            }
            match = false;
            for (auto &s : cfg->export_types) {
                if (s != exp) continue;

                match = true;
                break;
            }
            cbu::log_success("Added export type to config!");
            if (match) continue;

            cfg->export_types.push_back(exp);
            auto fmt = ConfigFileFormat();
            fmt.save_config(cfg);

            continue;
        } if (cmd == "x") {
            string name;
            cbu::cli_input("Enter config name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            bool match = false;
            cf *cfg;
            for (auto &s : current_config->loaded_configs) {
                if (s->name == name) {
                    match = true;
                    cfg = s;
                    break;
                }
            }
            if (!match) {
                cbu::log_warn("Config does not exist or is not linked!");
                continue;
            }

            cbu::cli_input("Enter package name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            ExternalObject obj{};

            cbu::cli_input("Package name in linux package managers (leave empty for none)");
            string pkg = cbu::cli_get_string();
            if (pkg.empty()) {
                cbu::log_error(false,"Not implemented");

                continue;
            } else {
                obj.linux_package.name = pkg;
                cbu::cli_input("Enter the shared library name (leave empty for header only)");
                obj.linux_package.link_name = cbu::cli_get_string();
                cbu::cli_input("Enter the include path");
                string inc;
                if (!cbu::cli_get_valid_string(&inc)) {
                    cbu::log_error(false,"Empty include path, using /usr/include as a default...");
                    inc = "/usr/include";
                }
                obj.linux_package.include_path = inc;
            }
            cbu::cli_input("URL for the mingw tar.gz download for windows, such as a github release (leave blank for none)");
            string win_url = cbu::cli_get_string();

            obj.win_ext_binary.download_url = win_url;
            if (obj.win_ext_binary) {
                cbu::cli_input("Enter the dll name (leave empty for header only)");
                obj.win_ext_binary.bin_name = cbu::cli_get_string();

                cbu::cli_output("Downloading archive...");
                uint8_t code = cbu::run_shell_command(mcc::compiler::get_temp_path(cfg),std::format("curl --fail --location --output archive.tar.gz {}",win_url),NULL);

                if (code) {
                    cbu::log_error(false,"Downloading archive failed, is the link invalid?");

                    continue;
                }

                cbu::cli_output("Extracting archive...");

                fpath apath = mcc::compiler::get_temp_path(cfg) / "archive";
                std::filesystem::create_directories(apath);
                code = cbu::run_shell_command(mcc::compiler::get_temp_path(cfg),"tar -xzf archive.tar.gz -C archive",NULL);
                if (code) {
                    cbu::log_error(false,"Extracting archive failed (for some reason)");

                    continue;
                }

                fpath p;
                cbu::cli_input("Select the include path");
                if (!cbu::cli_get_valid_dirpath(&p,apath)) {
                    cbu::log_warn("Cancelled");

                    continue;
                }
                p = std::filesystem::relative(p,apath);

                obj.win_ext_binary.include_path = p;
            }

            cfg->external_objects.push_back(new ExternalObject(obj));
            mcc::installer::install_all(cfg);
            update_config(cfg);

            continue;
        } if (cmd == "n") {
            string name;
            mcc::config::ConfigModel model;
            fpath project_path;
            fpath src_path;

            cbu::cli_input("Enter config name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            int mi;
            cbu::cli_input("---\nEnter config mode\nGeneric executable (0 default)\nShared library (1)\nExecutable with subprojects (2)\nEnter value");
            if (!cbu::cli_get_valid_int(&mi,0,2,true,0)) {
                cbu::log_error(false,"Invalid value");
                continue;
            }
            switch (mi) {
                case 0: {
                    model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
                    break;
                } case 1: {
                    model = mcc::config::ConfigModel::SINGLE_SHARED;
                    break;
                } case 2: {
                    model = mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED;
                    break;
                }
            }

            if (has_new_project_prepath) {
                project_path = new_project_prepath;
            } else {
                cbu::cli_output("---\nEnter the project path");
                if (!cbu::cli_get_valid_dirpath(&project_path)) {
                    cbu::log_error(false,"Cancelled");
                    continue;
                }
            }
            cbu::cli_output("---\nEnter the path for source files");
            if (!cbu::cli_get_valid_dirpath(&src_path,project_path)) {
                cbu::log_error(false,"Cancelled");
                continue;
            }
            src_path = std::filesystem::relative(src_path,project_path);

            config::ConfigObject *cfg = new config::ConfigObject();
            cbu::log_debug(std::format("Creating config {} at path {} with src {}",
                                       name,
                                       cbu::path_to_utf8(project_path),
                                       cbu::path_to_utf8(src_path)));
            cfg->name = name;
            cfg->directory = project_path;
            cfg->src_directory = src_path;
            cfg->model = model;
            update_config(cfg);

            link_config(cfg);
            cbu::log_success("Config created and linked succesfully!");

            continue;
        } if (cmd == "s") {
            if (!current_config) continue;
            cbu::cli_input("Enter config name to set current");
            string name = cbu::cli_get_string();

            set_name_current(name);

            continue;
        } if (cmd == "e") {
            string name;
            cbu::cli_input("Enter config name");
            if (!cbu::cli_get_valid_string(&name)) {
                cbu::log_error(false,"Name cannot be empty");
                continue;
            }
            bool match = false;
            cf *cfg;
            for (auto &s : current_config->loaded_configs) {
                if (s->name == name) {
                    match = true;
                    cfg = s;
                    break;
                }
            }
            if (!match) {
                cbu::log_warn("Config does not exist or is not linked!");
                continue;
            }

            bool edit_running = true;
            while (edit_running) {
                cbu::cli_output("Welcome to the config edit utility");
                cbu::cli_input("What to edit?\nExit edit utility (0)\nName (1 default)\nSrc Directory (2)\nModel (3)\nExternal Objects (4)");
                int opt;
                cbu::cli_get_valid_int(&opt,0,4,true,1);
                switch (opt) {
                    case 0: {
                        edit_running = false;
                        break;
                    }
                    case 1: {
                        string name;
                        cbu::cli_input("Enter new name");
                        if (!cbu::cli_get_valid_string(&name)) {
                            cbu::log_warn("Name cannot be empty");
                            break;
                        }
                        cfg->name = name;
                        update_config(cfg);

                        break;
                    }
                    case 2: {
                        fpath dir;
                        cbu::cli_input("Enter source directory");
                        if (!cbu::cli_get_valid_dirpath(&dir,cfg->directory)) {
                            cbu::log_warn("Canceled");
                            break;
                        }
                        cfg->src_directory = std::filesystem::relative(dir,cfg->directory);
                        update_config(cfg);

                        break;
                    }
                }
            }

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
bool mcc::config::link_file(fpath path) {
    if (!std::filesystem::exists(path)) {
        cbu::log_error(false,"File does not exist");
        return false;
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
    bool s = link_file(ppath);
    if (!s) return false;

    return set_name_current(last_config_name);
}
bool mcc::config::set_name_current(string name) {
    bool match = false;
    for (auto &s : current_config->loaded_configs) {
        if (s->name == name) {
            mcc::state::state_safe([s]() {
                mcc::state::active_config = s;
                mcc::state::current_project = s->directory;
            });

            cbu::log_success(std::format("Project {} at {} is now current",name,cbu::path_to_utf8(s->directory)));
            match = true;
            break;
        }
    }
    if (match) return true;
    cbu::log_warn(std::format("Project {} does not exist or is unlinked",name));

    return false;
}
