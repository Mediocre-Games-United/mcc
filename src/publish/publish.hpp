#pragma once
#include "config_file.hpp"
#include "version.hpp"
#include <cstdint>

namespace mcc::publisher {
    U8 publish_all(mcc::config::ConfigObject *cfg,mcc::version::Version version,bool noconfirm);
}
