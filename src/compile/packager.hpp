#pragma once

#include "compile/compiler.hpp"
#include "config/config_file.hpp"
#include "version.hpp"
#include <cstdint>

namespace mcc::packager {
    U8 package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt);
    U8 export_all(mcc::config::ConfigObject *cfg,mcc::version::Version version);
}
