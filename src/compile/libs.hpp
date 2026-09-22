#pragma once

#include "base_types.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include <cstdint>

namespace mcc::libs {
    uint8_t copy_libs(fpath build_path,fpath exe_file,mcc::config::ConfigObject *cfg,mcc::compiler::Platform pt);
}
