#include "libs.hpp"
#include "base_types.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include "base_string.hpp"
#include "version.hpp"
#include <filesystem>
#include <format>
#include <unordered_set>

using namespace mcc::config;

static string get_extract_cmd_win(fpath exe_file) {
    return std::format(
        "x86_64-w64-mingw32-objdump -p {} | awk '$1 == \"DLL\" && $2 == \"Name:\" {{ printf \"%s%c\", $3, 1 }}'",
        exe_file
    );
}

static string get_extract_cmd_linux(fpath exe_file) {
    return std::format(
        "ldd {} | awk 'NF {{ name=$1; sub(/^.*\\//,\"\",name); printf \"%s%c\", name, 1 }}'",
                       exe_file
    );
}

U8 mcc::libs::copy_libs(fpath target_path,fpath exe_file,mcc::config::ConfigObject *cfg,mcc::compiler::BuildType tp,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version,bool only_nontrivial) {
    fpath build_path = mcc::compiler::get_build_path(cfg,tp,pt,exp,version);
    cbu::log_info(std::format("Copying libs for {}...",exe_file));

    string output;
#ifdef _WIN32
    // windows link
#else
    fpath ln = cfg->directory / "latest";
    std::error_code ec;
    if (std::filesystem::is_symlink(ln,ec)) {
        if (ec) cbu::log_warn(ec.message());
        std::filesystem::remove(ln,ec);
        if (ec) cbu::log_warn(ec.message());
    }
    std::filesystem::create_directory_symlink((build_path / exe_file).parent_path(),ln,ec);
    if (ec) cbu::log_warn(ec.message());
#endif

    U8 code = 0;
    auto copy_sub = [cfg,pt,build_path,&ec,target_path,tp,exp,version]() {
        if (cfg->model != ConfigModel::SINGLE_EXECUTABLE) {
            string nm = pt->get_shared_name(cfg->get_raw_name());
            if (pt == &compiler::platform_linux) {
                fpath s = build_path / "lib" / nm;
                if (std::filesystem::exists(s)) std::filesystem::remove(s,ec);
                if (ec) cbu::log_warn(ec.message());
            }

            std::filesystem::copy_file(build_path / "out" / nm,target_path / nm,std::filesystem::copy_options::overwrite_existing,ec);
            if (ec) cbu::log_warn(ec.message());
        }
        for (auto &s : cfg->sub_projects) {
            fpath sub_build_path = mcc::compiler::get_build_path(s,tp,pt,exp,version);
            string nm = pt->get_shared_name(s->get_raw_name());
            if (pt == &compiler::platform_linux) {
                fpath s = build_path / "lib" / nm;
                if (std::filesystem::exists(s)) std::filesystem::remove(s,ec);
                if (ec) cbu::log_warn(ec.message());
            }

            std::filesystem::copy_file(sub_build_path / "out" / nm,target_path / nm,std::filesystem::copy_options::overwrite_existing,ec);
            if (ec) cbu::log_warn(ec.message());
        }
    };

    copy_sub();

    if (pt == &mcc::compiler::platform_linux) {
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
        }

        std::filesystem::create_directories(target_path / "lib");
        std::filesystem::create_directories(target_path / "extlib");
        string output = "";
        code = cbu::run_shell_command(build_path,get_extract_cmd_linux(exe_file),&output);

        vector<string> list = output.split("\1");

        cbu::log_verbose(output);
        for (auto &s : list) {
            cbu::log_info(std::format("Found lib {}",s));
            for (auto c : s) {
                cbu::log_debug(std::format("{} ({})",string(c),U32(c)));
            }
        }
        for (auto &s : list) {
            s.trim();
            cbu::log_debug(std::format("Found name {}",s));

            fpath cand = fpath("/usr/lib") / s;
            if (std::filesystem::exists(cand)) {
                std::error_code ec;
                std::filesystem::copy_file(cand,target_path / "lib" / s,std::filesystem::copy_options::overwrite_existing,ec);
                if (ec) cbu::log_warn(ec.message());
                continue;
            }

            bool match = false;
            for (auto &ext : external) {
                ExternalBinary bin = ext.obj.linux_ext_binary;
                if (!bin) continue;
                fpath b = mcc::compiler::get_external_binary_path(ext.cfg,ext.obj.name,pt) / "bin";

                cand = b / s;
                if (std::filesystem::exists(cand)) {
                    std::filesystem::copy_file(cand,target_path / "extlib" / s,ec);
                    match = true;
                    if (ec) cbu::log_warn(ec.message());
                    continue;
                }
            }

            if (match) continue;
            cbu::log_warn(std::format("Could not find {}",s));
        }
    } else if (pt == &mcc::compiler::platform_windows) {
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
            string nm = s->get_raw_name() + ".dll";

            std::error_code ec;
            std::filesystem::copy_file(bpath / nm,target_path / nm,std::filesystem::copy_options::overwrite_existing,ec);
            if (ec) cbu::log_warn(ec.message());

        }
        for (auto &e : external) {
            ExternalBinary bin = e.obj.win_ext_binary;
            if (!bin) continue;
            fpath b = mcc::compiler::get_external_binary_path(e.cfg,e.obj.name,pt) / "bin";
            // cbu::log_info(std::format("TLD: {}",cbu::path_to_utf8(tld)));

            string nm = bin.dll_name + ".dll";
            bool match = false;
            fpath cand = b / nm;
            if (std::filesystem::exists(cand)) {
                std::filesystem::copy_file(cand,target_path / nm,ec);
                match = true;
                if (ec) cbu::log_warn(ec.message());
            }

            if (match) continue;
            cbu::log_warn(std::format("Could not find {}",nm));
        }

        const fpath mingw_bin = "/usr/x86_64-w64-mingw32/bin";

        std::deque<string> pending;
        std::unordered_set<string> checked;

        // Start with the executable.
        pending.push_back(exe_file);

        while (!pending.empty()) {
            string current = std::move(pending.front());
            pending.pop_front();

            // Windows DLL names are case-insensitive.
            string key = current;
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (!checked.insert(key).second)
                continue;

            const fpath current_path = current;
            string output;

            code = cbu::run_shell_command(
                build_path,
                get_extract_cmd_win(current_path),
                                          &output
            );

            if (code != 0) {
                cbu::log_warn(std::format("Could not inspect {}", current));
                continue;
            }

            for (auto dep : output.split("\1")) {
                dep.trim();
                if (dep.empty())
                    continue;

                const fpath dep_name = dep;
                fpath dep_path = build_path / dep_name;

                if (!std::filesystem::exists(dep_path))
                    dep_path = mingw_bin / dep_name;

                if (!std::filesystem::exists(dep_path)) {
                    cbu::log_warn(std::format("DLL not found: {}", dep));
                    continue;
                }

                cbu::log_info(std::format("Found DLL dependency {}", dep));

                // Queue the resolved file so its own imports are checked.
                pending.push_back(dep_path);

                // Copy it to the destination. Existing files are overwritten.
                std::error_code ec;
                std::filesystem::copy_file(
                    dep_path,
                    target_path / dep_name,
                    std::filesystem::copy_options::overwrite_existing,
                    ec
                );
                if (ec)
                    cbu::log_warn(ec.message());
            }
        }
    }

    copy_sub();

    return code;
}
