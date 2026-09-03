#pragma once

#include "cpp-baseutils/include/logger.hpp"
#include "cpp-baseutils/include/stringmath.hpp"
#include <string>
#include <cstdint>
#include <unordered_map>

namespace mcc {
    static std::unordered_map<baseutils::StringName,bool> commands;

    template<typename ...T>
    inline void add_command(std::string name,uint8_t (*callback)(T...)) {
        if (name.empty()) baseutils::log_error(true,"Cannot add empty command");
    }
}
