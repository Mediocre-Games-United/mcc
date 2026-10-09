#pragma once

#include "base_types.hpp"

namespace mcc::git {
    U8 sync();
    U8 undo();
    U8 status();
    U8 compare();
    U8 commit_all(string msg);
    U8 commit_all_sub(string sub,string msg);
    U8 change_branch(string target);
    U8 new_branch(string from,string name);
    U8 merge(string base,string feature);
    U8 fix();
    U8 clone(string target,string url);
}
