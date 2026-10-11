#pragma once
#include "config_file.hpp"
#include "version.hpp"
#include <cstdint>

namespace mcc::publisher {
    U8 publish_all(mcc::config::ConfigObject *cfg,bool noconfirm);
}
