#pragma once
#include "config_file.hpp"
#include <cstdint>

namespace mcc::publisher {
    uint8_t publish_all(mcc::config::ConfigObject *cfg,string version,bool noconfirm);
}
