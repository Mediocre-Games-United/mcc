#pragma once
#include "binary.hpp"
#include "config_file.hpp"

using cf = mcc::config::ConfigObject;

class ConfigFileFormatV2 : public cbu::BinaryFileVersion {
public:
    cbu::BinaryFileSection *get_sections() override;
};
class ConfigFileFormatV1 : public cbu::BinaryFileVersion {
public:
    cbu::BinaryFileSection *get_sections() override;
};
class ConfigFileFormat : public cbu::BinaryFileFormat {
public:
    ConfigFileFormat() : cbu::BinaryFileFormat({new ConfigFileFormatV1(),new ConfigFileFormatV2()},"mcc") {}

    cf *load_config(fpath path);
    void save_config(cf *obj);
};
