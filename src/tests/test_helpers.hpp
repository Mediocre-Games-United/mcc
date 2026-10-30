#pragma once

#include "config_file.hpp"
#include "init.hpp"

inline void wrap_bareminimum(std::function<void()> cb) {
    cbu::init();
    mcc::config::init();

    cb();
    cbu::deinit();
}
