#pragma once

#include "base_types.hpp"
#include "compiler.hpp"
#include "config/config_file.hpp"
#include <functional>

namespace mcc::state {
    extern fpath current_project;
    extern mcc::config::ConfigObject *active_config;
    extern mcc::compiler::BuildType active_build;
    extern mcc::compiler::Platform active_platform;

    void state_safe(std::function<void()> callback);
}
