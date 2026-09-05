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
                    p[i].fpath = cfg->directory / "project.mcc";
                }

                return p;
            },[](void *obj) { delete[] (ConfigContainerDataBlock*) obj; },{
                new cbu::DataBinarySection([](void *u,void *d) { // config file path
                    auto *ct = (ConfigContainer*) u;
                    ct->pending_datablocks.push((ConfigContainerDataBlock*) d);
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
    ConfigContainer *load_configs() {
        ConfigContainer *ct = new ConfigContainer();
        load_file_to_buffer(cbu::resolve_path("user://configs.cfg_ctr"));
        load_buffer_to_object(ct);
        size_t config_count = 0;
        while (!ct->pending_datablocks.empty()) {
            auto ref = ct->pending_datablocks.front();

            config_count += 1;
            cbu::log_verbose(std::format("Loading config id {} at {}",config_count,ref->fpath));

            delete ref;
            ct->pending_datablocks.pop();
        }
        cbu::log_debug(std::format("Loaded {} config files",config_count));

        return ct;
    }
};

using cf = mcc::config::ConfigObject;
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
            })
        });
    }
};
class ConfigFileFormat : public cbu::BinaryFileFormat {
public:
    ConfigFileFormat() : cbu::BinaryFileFormat({new ConfigFileFormatV1()},"mcc") {}

    cf *load_config(fpath path) {
        cf *obj = new cf();
        load_file_to_buffer(path);
        load_buffer_to_object(obj);
        obj->directory = path.parent_path();

        return obj;
    }
};

static ConfigContainer *current_config = NULL;
static void scan_config() {
    cbu::log_verbose("Scanning local config file");
    auto fmt = ConfigContainerFileFormat();
    if (current_config) delete current_config;

    current_config = fmt.load_configs();
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

static bool activate_config(mcc::config::ConfigObject *obj) {
    if (!current_config) return false;

    current_config->loaded_configs.push_back(obj);
    return true;
}

uint8_t mcc::config::cmd() {
    cbu::cli_output("Welcome to the config utility!");
    string cmd;
    while (true) {
        cbu::cli_input("CONFIG: Enter command (h for help)");
        cmd = cbu::cli_get_string();

        if (cmd == "h") {
            cbu::cli_output("[l] list configs\n[n] new config\n[e] edit existing\n[d] delete existing\n[a] add existing config\n[q] quit config utility");
            continue;
        } if (cmd == "q") {
            cbu::cli_output("Exiting config utility...");
            break;
        } if (cmd == "l") {
            if (!current_config or current_config->loaded_configs.empty()) {
                cbu::log_warn("No configs found! Add some with a or create a new one with n");
                continue;
            }
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
            cbu::cli_input("---\nEnter config mode\nGeneric monolithic executable (1)\nShared library (2)\nExecutable with subprojects (3)\nEnter value");
            if (!cbu::cli_get_valid_int(&mi,1,3)) {
                cbu::log_error(false,"Invalid value");
                continue;
            }
            switch (mi) {
                case 1: {
                    model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
                    break;
                } case 2: {
                    model = mcc::config::ConfigModel::SINGLE_SHARED;
                    break;
                } case 3: {
                    model = mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED;
                    break;
                }
            }
            cbu::cli_output("---\nEnter the project path");
            if (!cbu::cli_get_valid_dirpath(&project_path)) {
                cbu::log_error(false,"Cancelled");
                continue;
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
            cfg->directory = project_path;
            cfg->src_directory = src_path;


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
