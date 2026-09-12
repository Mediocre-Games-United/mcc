#pragma once

#include "compile/compiler.hpp"
#include "config/config_file.hpp"
#include <cstdint>

namespace mcc::packager {
    uint8_t package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt);
    uint8_t export_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt);
}
