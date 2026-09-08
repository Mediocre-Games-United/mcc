#pragma once

#include "base_types.hpp"
#include "config/config_file.hpp"
#include <functional>

namespace mcc::state {
    extern fpath current_project;
    extern mcc::config::ConfigObject *current_config;
    void state_safe(std::function<void()> callback);
}
