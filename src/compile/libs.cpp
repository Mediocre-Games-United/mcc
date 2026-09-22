#include "libs.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include <format>

uint8_t mcc::libs::copy_libs(fpath build_path,fpath exe_file,mcc::config::ConfigObject *cfg,mcc::compiler::Platform pt) {
    cbu::log_info("Copying libs...");

    string output = "";
    uint8_t code = 0;
    switch (pt) {
        case mcc::compiler::Platform::PLATFORM_LINUX: {
            code = cbu::run_shell_command(build_path,std::format("/usr/bin/ldd {}",cbu::path_to_utf8(exe_file)),&output);
            // cbu::log_info(output);
            string name;
            string path;

            bool is_name = true;
            bool is_path = false;

            size_t switch_stage = 0;
            for (size_t i = 0; i < output.size(); i ++) {
                char c = output[i];
                if (c == '\n') {
                    switch_stage = 0;
                    continue;
                } if (c == ' ') {
                    switch_stage = 0;
                    continue;
                } if (switch_stage == 0 && c == '=') {
                    switch_stage = 1;
                    continue;
                } if (switch_stage == 1 && c == '>') {
                    switch_stage = 0;
                    is_name = false;
                    is_path = true;
                    continue;
                } if (c == '(') {
                    cbu::log_debug(std::format("Found name {} with path {}",name,path));

                    is_path = false;
                    continue;
                } if (c == ')') {
                    is_name = true;
                    name = "";
                    path = "";
                    continue;
                }

                if (is_name) name += c;
                else if (is_path) path += c;
            }

            break;
        }
        case mcc::compiler::Platform::PLATFORM_WINDOWS: {
            break;
        }

        default: {}
    }

    return code;
}
