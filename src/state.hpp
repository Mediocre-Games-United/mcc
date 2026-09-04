#pragma once

#include "base_types.hpp"
#include <functional>

namespace mcc {
    extern fpath current_project;
    void state_safe(std::function<void()> callback);
}
