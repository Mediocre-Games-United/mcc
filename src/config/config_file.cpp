#include "base_types.hpp"
#include "config_file.hpp"
#include "cli.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "binary.hpp"
#include <filesystem>
#include <format>
#include <queue>


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
static bool activate_config(mcc::config::ConfigObject *obj);
static void scan_config();
static void update_config_src(cf *obj);


class ConfigFileFormatV1 : public cbu::BinaryFileVersion {
public:
    cbu::BinaryFileSection *get_sections() override {
        return new cbu::MainBinaryFileSection({
            new cbu::StringBinarySection([](void *obj,auto value) { // name
                cf *c = (cf*) obj;
                c->name = value;
            },[](void *obj) -> string {
                return ((cf*) obj)->name;
            }),
            new cbu::StringBinarySection([](void *obj,auto value) { // srcdir
                cf *c = (cf*) obj;
                c->src_directory = value;
            },[](void *obj) -> string {
                return ((cf*) obj)->src_directory;
            }),
            new cbu::U8BinarySection([](void *obj,auto value) { // model
                cf *c = (cf*) obj;
                mcc::config::ConfigModel model;
                switch (value) {
                    case 0: {
                        model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
                        break;
                    }
                    case 1: {
                        model = mcc::config::ConfigModel::SINGLE_SHARED;
                        break;
                    }
                    case 2: {
                        model = mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED;
                        break;
                    }
                    default: {
                        throw std::format("Invalid config model {}",value);
                    }
                }

                c->model = model;
            },[](void *obj) -> uint8_t {
                return (uint8_t) ((cf*) obj)->model;
            }),
            new cbu::DataBinarySection([](void *obj,void *data) {
                auto *co = (mcc::config::SourceFileObject*) data;
                auto *c = (cf*) obj;

                if (co->name.empty()) return;
                c->main_source = new mcc::config::SourceFileObject(*co);
            },[]() -> void* {
                return new mcc::config::SourceFileObject();
            },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
                new cbu::StringBinarySection([](void *obj,auto value) { // main src path
                    auto *c = (mcc::config::SourceFileObject*) obj;
                    c->path = value;
                },[](void *obj) -> string {
                    auto *c = (cf*) obj;
                    if (c->main_source) return cbu::path_to_utf8(c->main_source->path);

                    return "";
                }),
            }),
            new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* {
                auto *obj = (cf*) u;
                *len = obj->source_files.size();
                *size = sizeof(mcc::config::SourceFileObject*);

                for (auto &s : obj->source_files) {
                    cbu::log_debug(std::format("Src: {}",(void*) s));
                }
                if (*len == 0) return NULL;
                return obj->source_files.data();
            },[](void *obj) {},{
                new cbu::DataBinarySection([](void *obj, void *data) {
                    auto *co = (mcc::config::SourceFileObject*) data;
                    auto *c = (cf*) obj;

                    c->source_files.push_back(new mcc::config::SourceFileObject(*co));
                },[]() -> void* { return new mcc::config::SourceFileObject(); },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
                    new cbu::StringBinarySection([](void *obj,auto value) { // src path
                        auto *c = (mcc::config::SourceFileObject*) obj;
                        c->path = value;
                    },[](void *obj) -> string {
                        return cbu::path_to_utf8((*(mcc::config::SourceFileObject**) obj)->path);
                    }),
                    new cbu::U8BinarySection([](void *obj,auto value) {
                        auto *c = (mcc::config::SourceFileObject*) obj;
                        c->enabled = bool(value);
                    },[](void *obj) -> uint8_t {
                        return uint8_t((*(mcc::config::SourceFileObject**) obj)->enabled);
                    })
                })
            },false)
        });
    }
};
class ConfigFileFormat : public cbu::BinaryFileFormat {
public:
    ConfigFileFormat() : cbu::BinaryFileFormat({new ConfigFileFormatV1()},"mcc") {}

    cf *load_config(fpath path) {
        cf *obj = new cf();
        obj->directory = path.parent_path();
        load_file_to_buffer(path);
        load_buffer_to_object(obj);

        return obj;
    }
    void save_config(cf *obj) {
        load_object_to_buffer(obj);
        save_buffer_to_file(obj->directory / mcc::config::FNAME);
    }
};

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
                    p[i].fpath = cfg->directory / mcc::config::FNAME;
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
                update_config_src(cfg);
                activate_config(cfg);
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

    fmt.load_configs();
}

static bool activate_config(mcc::config::ConfigObject *obj) {
    if (!current_config) return false;

    current_config->loaded_configs.push_back(obj);
    auto fmt = ConfigContainerFileFormat();
    fmt.save_configs();
    return true;
}

static void config_recurse_src_files(vector<cf*> &subconfigs,mcc::config::SourceFileObject *&main,vector<mcc::config::SourceFileObject*> &sourcefiles,fpath root,fpath path) {
    bool no_match = true;
    for (auto &s : subconfigs) {
        if (path == s->directory) {
            no_match = false;
            break;
        }
    }
    if (!no_match) return;

    for (auto &s : cbu::iterate_dir(path)) {
        if (std::filesystem::is_directory(s)) {
            config_recurse_src_files(subconfigs,main,sourcefiles,root,s);
            continue;
        }
        if (!std::filesystem::is_regular_file(s)) continue;
        string ext = s.extension().string();
        if (ext == ".cpp" || ext == ".c") {
            string name = s.stem();
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
            sourcefiles.push_back(new mcc::config::SourceFileObject{
                .path = p,
                .name = s.parent_path() / name
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

        cbu::log_debug(std::format("Found {} at {}",mcc::config::FNAME,cbu::path_to_utf8(s)));
        auto fmt = ConfigFileFormat();
        cf *cfg = fmt.load_config(s);
        update_config_src(cfg);

        activate_config(cfg);
        subconfigs.push_back(cfg);

        continue;
    }
}
static void update_config_src(cf *obj) {
    vector<cf*> subconfigs = obj->sub_projects;
    vector<mcc::config::SourceFileObject*> sourcefiles = obj->source_files;

    mcc::config::SourceFileObject *main = NULL;
    auto lpath = obj->directory / obj->src_directory;
    config_recurse_sub_projects(subconfigs,lpath);
    config_recurse_src_files(subconfigs,main,sourcefiles,lpath,lpath);

    obj->source_files = sourcefiles;
    obj->sub_projects = subconfigs;

    obj->main_source = main;
}

void mcc::config::init() {
    scan_config();
}
uint8_t mcc::config::reload() {
    scan_config();

    return 0;
}
void mcc::config::background() {

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
            cbu::cli_output("[l] list configs\n[n] new config\n[e] edit existing\n[a] link existing config\n[q] quit config utility");
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
                    cbu::log_warn("Project is an invalid file");
                    continue;
                }
                auto fmt = ConfigFileFormat();
                cf *cfg = fmt.load_config(ppath);
                update_config_src(cfg);
                activate_config(cfg);

                continue;
            }
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
            update_config_src(cfg);

            auto fmt = ConfigFileFormat();
            fmt.save_config(cfg);

            activate_config(cfg);
            cbu::log_success("Config created and activated succesfully!");

            continue;
        }

        cbu::log_warn("Unknown config command");
    }

    return 0;
}
bool mcc::config::valid() {
    return false;
}
