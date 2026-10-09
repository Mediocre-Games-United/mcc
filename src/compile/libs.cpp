#include "libs.hpp"
#include "base_types.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "stringmath.hpp"
#include "version.hpp"
#include <filesystem>
#include <format>

using namespace mcc::config;

static string get_extract_cmd_win(fpath exe_file) {
    return std::format("/usr/bin/x86_64-w64-mingw32-objdump -p {} | awk \'$1 == \"DLL\" && $2 == \"Name:\" {{ print $3 }}\'",cbu::path_to_utf8(exe_file));
}
U8 mcc::libs::copy_libs(fpath exe_file,mcc::config::ConfigObject *cfg,mcc::compiler::BuildType tp,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version) {
    fpath build_path = mcc::compiler::get_build_path(cfg,tp,pt,exp,version);
    cbu::log_info("Copying libs...");

    string output = "";
    U8 code = 0;
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
                } if (c == '\t') {
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

                    std::error_code ec;
                    fpath npath = fpath(name);
                    string nm = cbu::path_to_utf8(npath.filename());
                    if (std::filesystem::exists(npath)) {
                        cbu::log_verbose("Name is a valid path");

                        std::filesystem::copy_file(npath,build_path / nm,std::filesystem::copy_options::overwrite_existing,ec);
                        if (ec) cbu::log_warn(ec.message());
                        continue;
                    }
                    if (path.empty()) {
                        cbu::log_verbose("Empty path, skipping");
                        continue;
                    }
                    if (!std::filesystem::exists(path)) continue;

                    std::filesystem::copy_file(path,build_path / nm,std::filesystem::copy_options::overwrite_existing,ec);
                    if (ec) cbu::log_warn(ec.message());
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
            vector<ExternalWrapper> external{};
            for (auto &e : cfg->external_objects) {
                external.push_back(ExternalWrapper{
                    .obj = e,
                    .cfg = cfg
                });
            }
            for (auto &s : cfg->sub_projects) {
                for (auto &e : s->external_objects) {
                    external.push_back(ExternalWrapper{
                        .obj = e,
                        .cfg = s
                    });
                }

                fpath bpath = mcc::compiler::get_build_path(s,tp,pt,exp,version);
                string nm = s->name + ".dll";

                std::error_code ec;
                std::filesystem::copy_file(bpath / nm,build_path / nm,ec);
                if (ec) cbu::log_warn(ec.message());

            }
            for (auto &e : external) {
                ExternalBinary bin = e.obj->win_ext_binary;
                if (!bin) continue;
                fpath tld = mcc::compiler::get_external_binary_path(e.cfg,e.obj->name,mcc::compiler::Platform::PLATFORM_WINDOWS) / "extracted" / bin.tld;
                // cbu::log_info(std::format("TLD: {}",cbu::path_to_utf8(tld)));

                string nm = bin.bin_name + ".dll";
                fpath cand = tld / "lib" / nm;
                std::error_code ec;
                if (std::filesystem::exists(cand)) {
                    std::filesystem::copy_file(cand,build_path / nm,ec);
                    if (ec) cbu::log_warn(ec.message());
                    continue;
                }
                cand = tld / "bin" / nm;
                if (!std::filesystem::exists(cand)) {
                    cbu::log_error(false,std::format("Could not find {}.dll",bin.bin_name));
                    return -1;
                }

                std::filesystem::copy_file(cand,build_path / nm,ec);
                if (ec) cbu::log_warn(ec.message());
            }

            // the harder ones
            string output = "";
            code = cbu::run_shell_command(build_path,get_extract_cmd_win(exe_file),&output);

            vector<string> list = cbu::string_split(output,"\n");
            vector<string> temp{};
            for (auto &s : list) {
                cbu::trim(s);
                fpath tpath = build_path / s;
                if (!std::filesystem::exists(tpath)) continue;
                code = cbu::run_shell_command(build_path,get_extract_cmd_win(tpath),&output);

                vector<string> list = cbu::string_split(output,"\n");
                for (auto &e : list) { temp.push_back(e); }
            }
            for (auto &s : temp) { list.push_back(s); }

            for (auto &s : list) {
                cbu::trim(s);
                fpath tpath = fpath("/usr/x86_64-w64-mingw32/bin") / s;

                std::error_code ec;
                std::filesystem::copy_file(tpath,build_path / s,ec);
                if (ec) cbu::log_warn(ec.message());
            }

            break;
        }

        default: {}
    }

    return code;
}
