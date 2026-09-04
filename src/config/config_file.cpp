#include "base_types.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "binary.hpp"
#include <queue>

struct ConfigObject {

};
struct ConfigContainerDataBlock {
    string fpath;
};
struct ConfigContainer {
    std::vector<ConfigObject> loaded_configs;
    std::queue<ConfigContainerDataBlock> pending_datablocks;
};
class ConfigContainerFileFormatV1 : public baseutils::BinaryFileVersion {
public:
    baseutils::BinaryFileSection *get_sections() override {
        return new baseutils::MainBinaryFileSection({
            new baseutils::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* {
                return NULL;
            },[](void *obj) {

            },{
                new baseutils::DataBinarySection([](void *u,void *d) {

                },[]() -> void* { return new ConfigContainerDataBlock(); },[](void *obj) { delete (ConfigContainerDataBlock*) obj; },{
                    new baseutils::StringBinarySection([](void *obj,auto value) {
                        auto *db = (ConfigContainerDataBlock*) obj;
                        db->fpath = value;
                    },[](void *obj) -> string {

                    }) // config file path
                })
            })
        });
    }
};
class ConfigContainerFileFormat : public baseutils::BinaryFileFormat {
public:
    ConfigContainerFileFormat() : BinaryFileFormat({new ConfigContainerFileFormatV1()},"cfg_ctr") {};

};

static void scan_config() {
    baseutils::log_verbose("Scanning local config file...");

}
void mcc::config::init() {
    scan_config();
}
void mcc::config::background() {

}
uint8_t mcc::config::cmd() {
    return 0;
}
bool mcc::config::valid() {
    return false;
}
