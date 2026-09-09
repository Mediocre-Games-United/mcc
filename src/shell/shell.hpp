#pragma once

#include "base_types.hpp"

namespace mcc::shell {
    uint8_t run_shell_command(fpath cwd,string cmd,string *output);
}
