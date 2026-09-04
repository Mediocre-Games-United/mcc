#pragma once

#include "base_types.hpp"
#include "logger.hpp"
#include "stringmath.hpp"
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "stringmath.hpp"
#include <format>
#include <tuple>
#include <functional>

namespace mcc {
    struct cmdinfo {
        string help_example;
        string name;
        std::function<uint8_t(std::vector<string>)> dispatcher;
    };
    template<typename ...T>
    struct cmdexecutor {
        inline static string get_help(string name,std::vector<string> help_lines,string desc) {
            std::vector<string> list = { print_field<T>()... };
            string str = std::format("Usage: {} ",name);
            for (size_t i = 0; i < list.size(); i ++) {
                string type = list[i];
                if (i < help_lines.size()) str = std::format("{} [{}: {}]",str,help_lines[i],type);
                else str = std::format("{} [{}]",str,type);
            }

            if (desc.empty()) return str;
            return std::format("{}\n{}",str,desc);
        }
        inline static uint8_t decode(std::tuple<T...> *target,std::vector<string> vars) {
            uint8_t res = decode_impl(target,vars);
            return res;
        }
    private:
        template<typename U>
        inline static string print_field() {
            if constexpr (std::is_same_v<U,int>) return "int";

            assert(!"Invalid type for commands");
        }
        template<typename U>
        inline static U decode_field(std::vector<string> vars,size_t &offset) {
            if (offset >= vars.size()) baseutils::log_error(true,std::format("Argument {} cannot be empty",offset + 1));
            if constexpr (std::is_same_v<U,int>) {
                int i = atoi(vars[offset].c_str());
                offset += 1;
                return i;
            }

            assert(!"Invalid type for commands");
        }
        inline int8_t static decode_impl(std::tuple<T...> *target,std::vector<string> vars) {
            size_t offset = 0;
            try {
                auto t = std::tuple<T...>{
                    decode_field<T>(
                        vars,offset
                    )...
                };

                *target = t;
                return 0;
            } catch (std::runtime_error &err) {
                return -1;
            }
        }
    };
    using command_table = std::unordered_map<baseutils::StringName,cmdinfo>;
    inline command_table &get_commands() {
        static command_table commands{};

        return commands;
    }

    template<typename ...T>
    inline void add_command(string name,uint8_t (*callback)(T...),std::vector<string> help_lines = {},string help = "") {
        if (name.empty()) baseutils::log_error(true,"Cannot add empty command");
        baseutils::log_verbose(std::format("Adding command {}",name));

        command_table &commands = get_commands();
        commands[name] = cmdinfo{
            .help_example = cmdexecutor<T...>::get_help(name,help_lines,help),
            .name = name,
            .dispatcher = [callback,name](std::vector<string> vars) -> uint8_t {
                std::tuple<T...> args{};
                uint8_t res = cmdexecutor<T...>::decode(&args,vars);
                if (res) return res;
                uint8_t r = std::apply(callback,args);
                string txt = std::format("Command {} returned code {}",name,r);
                if (r) baseutils::log_error(false,txt);
                else baseutils::log_success(txt);

                return r;
            }
        };
    }
    inline std::vector<cmdinfo> list_commands() {
        std::vector<cmdinfo> list{};
        command_table &commands = get_commands();
        for (auto &s : commands) {
            list.push_back(s.second);
        }

        return list;
    }
    inline uint8_t run_command(string input) {
        baseutils::log_verbose(std::format("Input: {}",input));
        auto split = baseutils::string_split(input," ");
        if (split.empty()) {
            baseutils::log_error(false,"Command cannot be empty");
            return -1;
        }
        string cmd = split[0];
        split.erase(split.begin());

        baseutils::log_verbose(std::format("Cmd: {}",cmd));
        if (cmd == "exit" || cmd == "logout" || cmd == "quit" || cmd == "q") { return 137; }

        command_table &commands = get_commands();
        if (!commands.contains(cmd)) {
            baseutils::log_warn(std::format("Command {} not found, type help for available commands",cmd));
            return -1;
        }

        return commands[cmd].dispatcher(split);
    }
}
