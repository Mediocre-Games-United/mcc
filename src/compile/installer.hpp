#pragma once

#include "config_file.hpp"
#include <cstdint>

namespace mcc::installer {
    uint8_t install_all(mcc::config::ConfigObject *cfg);
}
