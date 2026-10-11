#pragma once

#include "base_types.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "version.hpp"
#include <cstdint>

namespace mcc::libs {
    U8 copy_libs(fpath exe_file,mcc::config::ConfigObject *cfg,mcc::compiler::BuildType tp,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version);
}
