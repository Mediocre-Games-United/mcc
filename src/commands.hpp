#pragma once

#include "base_types.hpp"
#include "logger.hpp"
#include "stringmath.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "stringmath.hpp"

namespace mcc {
    struct cmdinfo {
        string help_example;
        string help_text;
    };
    static std::unordered_map<baseutils::StringName,cmdinfo> commands;

    template<typename ...T>
    inline void add_command(string name,uint8_t (*callback)(T...)) {
        if (name.empty()) baseutils::log_error(true,"Cannot add empty command");

        commands[name] = cmdinfo{
            .help_example = name,
            .help_text = U""
        };
    }
    inline std::vector<cmdinfo> get_commands() {
        std::vector<cmdinfo> list{};
        for (auto &s : commands) {
            list.push_back(s.second);
        }

        return list;
    }
    inline uint8_t run_command(string input) {
        auto split = baseutils::string_split(input,U" ");
        if (split.empty()) {
            baseutils::log_error(false,"Command cannot be empty");
            return -1;
        }
        string cmd = split[0];

    }
}
