#pragma once

#include "compile/compiler.hpp"
#include "config/config_file.hpp"
#include "version.hpp"

namespace mcc::packager {
    U8 package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version);
    U8 export_all(mcc::config::ConfigObject *cfg);
}
