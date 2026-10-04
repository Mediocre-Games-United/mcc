#pragma once

#include "config_file.hpp"
#include <cstdint>

namespace mcc::installer {
    U8 install_all(mcc::config::ConfigObject *cfg);
}
