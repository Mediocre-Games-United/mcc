#pragma once

#include "base_types.hpp"

namespace mcc::git {
    U8 sync();
    U8 undo();
    U8 status();
    U8 compare();
    U8 commit_all(string msg);
    U8 commit_all_sub(string sub,string msg);
}
