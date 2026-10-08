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
#include <mutex>

namespace mcc {
    extern volatile size_t interrupt_count;
    extern std::mutex interrupt_mutex;
    inline bool consume_interrupt() {
        interrupt_mutex.lock();
        if (interrupt_count <= 0) { interrupt_mutex.unlock(); return false; }

        cbu::log_warn("Interrupt caught!");
        interrupt_count -= 1;
        interrupt_mutex.unlock();
        return true;
    }

    struct cmdinfo {
        string help_example;
        string name;
        std::function<U8(std::vector<string>)> dispatcher;
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
        inline static U8 decode(std::tuple<T...> *target,std::vector<string> vars) {
            U8 res = decode_impl(target,vars);
            return res;
        }
    private:
        template<typename U>
        inline static string print_field() {
            if constexpr (std::is_same_v<U,int>) return "int";
            if constexpr (std::is_same_v<U,string>) return "string";

            assert(!"Invalid type for commands");
        }
        template<typename U>
        inline static U decode_field(std::vector<string> vars,size_t &offset) {
            if (offset >= vars.size()) cbu::log_error(true,std::format("Argument {} cannot be empty",offset + 1));
            if constexpr (std::is_same_v<U,int>) {
                int i = atoi(vars[offset].c_str());
                offset += 1;
                cbu::log_verbose(std::format("Found int {}",i));
                return i;
            } if constexpr (std::is_same_v<U,string>) {
                string i = vars[offset];
                offset += 1;
                cbu::log_verbose(std::format("Found string {}",i));
                return i;
            }

            assert(!"Invalid type for commands");
        }
        inline U8 static decode_impl(std::tuple<T...> *target,std::vector<string> vars) {
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
    using command_table = std::unordered_map<cbu::StringName,cmdinfo>;
    inline command_table &get_commands() {
        static command_table commands{};

        return commands;
    }

    template<typename ...T>
    inline void add_command(string name,U8 (*callback)(T...),std::vector<string> help_lines = {},string help = "") {
        if (name.empty()) cbu::log_error(true,"Cannot add empty command");
        cbu::log_verbose(std::format("Adding command {}",name));

        command_table &commands = get_commands();
        commands[name] = cmdinfo{
            .help_example = cmdexecutor<T...>::get_help(name,help_lines,help),
            .name = name,
            .dispatcher = [callback,name](std::vector<string> vars) -> U8 {
                constexpr std::size_t arg_count = sizeof...(T);
                using Args = std::tuple<T...>;

                if constexpr (arg_count > 0) {
                    using LastArg = std::remove_cvref_t<
                    std::tuple_element_t<arg_count - 1, Args>
                    >;

                    if constexpr (std::is_same_v<LastArg, std::string>) {
                        constexpr std::size_t last_arg = arg_count - 1;

                        if (vars.size() > arg_count) {
                            std::string joined = vars[last_arg];
                            for (std::size_t i = arg_count; i < vars.size(); ++i) {
                                joined += ' ';
                                joined += vars[i];
                            }

                            vars.resize(arg_count);
                            vars[last_arg] = std::move(joined);
                        }
                    }
                }

                Args args{};
                U8 res = cmdexecutor<T...>::decode(&args, vars);
                if (res) return res;

                U8 r = std::apply(callback, args);
                string txt = std::format("Command {} returned code {}", name, r);

                if (r) cbu::log_error(false, txt);
                else cbu::log_success(txt);

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
    inline U8 run_command(string input) {
        cbu::log_verbose(std::format("Input: {}",input));
        vector<string> split{};
        bool has_quote = false;
        string cur = "";
        for (char c : input) {
            if (c == ' ' and !has_quote) {
                split.push_back(cur);
                cur.clear();
                continue;
            } if (c == '"') {
                has_quote = !has_quote;
                continue;
            }
            cur += c;
        }
        if (!cur.empty()) split.push_back(cur);

        if (split.empty()) {
            cbu::log_error(false,"Command cannot be empty");
            return -1;
        }
        for (string s : split) {
            cbu::log_verbose(std::format("Split {}",s));
        }
        string cmd = split[0];
        split.erase(split.begin());

        cbu::log_verbose(std::format("Cmd: {}",cmd));
        if (cmd == "exit" || cmd == "logout" || cmd == "quit" || cmd == "q") { return 137; }

        command_table &commands = get_commands();
        if (!commands.contains(cmd)) {
            cbu::log_warn(std::format("Command {} not found, type help for available commands",cmd));
            return -1;
        }

        return commands[cmd].dispatcher(split);
    }
}
