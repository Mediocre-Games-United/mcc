#include "installer.hpp"
#include "archive.hpp"
#include "commands.hpp"
#include "compiler.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include "shell.hpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>
#include "file.hpp"

using namespace mcc::config;
static U8 install_linux_package(ExternalPackage pck) {
    U8 code;
    string output;

    string test_cmd,install_cmd = "";
    if (std::filesystem::exists("/usr/bin/pacman")) {
        test_cmd = std::format("pacman -Q {}",pck.name);
        install_cmd = std::format("sudo pacman -Sy {}",pck.name);
    }
    if (std::filesystem::exists("/usr/bin/apt-get")) {
        test_cmd = std::format(
            "dpkg-query -W -f='${{Status}}' {} 2>/dev/null | "
            "grep -q '^install ok installed$'",
            pck.name
        );
        install_cmd = std::format("sudo apt-get {}",pck.name);
    }

    if (test_cmd.empty()) {
        cbu::log_error(false,"No supported package managers found");

        return -1;
    }
    code = cbu::run_shell_command("/",test_cmd,&output);
    if (code) code = cbu::run_shell_command("/",install_cmd,NULL);
    else cbu::log_info(std::format("{} is already installed",pck.name));

    if (code) {
        cbu::log_error(false,std::format("Failed to install {}",pck.name));

        return -1;
    }

    return 0;
}

static bool find_copy_from_dir(fpath targetdir,fpath searchdir,string name) {
    for (auto &p : cbu::iterate_dir(searchdir)) {
        if (p.is_directory()) {
            if (find_copy_from_dir(targetdir,p,name)) return true;

            continue;
        }

        if (p.filename() != name) continue;

        std::error_code ec;
        std::filesystem::copy_file(p,targetdir / name,std::filesystem::copy_options::overwrite_existing,ec);
        return true;
    }

    return false;
}
static U8 install_binary_package(ConfigObject *cfg,string name,ExternalBinary bin,mcc::compiler::Platform pt) {
    fpath root = mcc::compiler::get_external_binary_path(cfg,name,pt);
    fpath apath = root / "archive.tar.gz";
    fpath dpath = root / "extracted";
    fpath binpath = root / "bin";
    fpath incpath = root / "include";
    std::filesystem::create_directories(binpath);

    fpath tldpath = dpath / bin.tld;
    U8 code;
    if (!std::filesystem::exists(apath)) {
        cbu::log_info("Downloading archive");
        code = cbu::run_shell_command(root,std::format("curl --fail --location --output {} {}",apath,bin.download_url),NULL);
        if (code) {
            cbu::log_error(false,"Failed to download archive!");
            return -1;
        }
    }
    if (!std::filesystem::exists(dpath)) {
        cbu::log_info("Extracting archive");
        std::filesystem::create_directories(dpath);
        auto archive = ArchiveTarGZ();
        if (!archive.extract_archive_single(apath,dpath)) {
            cbu::log_error(false,"Failed to extract archive for some reason, maybe an issue with permissions?");
            return -1;
        }
    }
    if (!std::filesystem::exists(tldpath)) {
        cbu::log_error(false,std::format("TLD Path is invalid! {}",tldpath));

        return -1;
    }
    if (std::filesystem::exists(tldpath / ".cmd_ran")) {
        cbu::log_verbose("Install cmd already ran!");
    } else if (!bin.install_cmd.empty()) {
        code = cbu::run_shell_command(tldpath,bin.install_cmd,NULL);
        if (code) {
            cbu::log_error(false,"Install command failed");
            return -1;
        }

        std::ofstream(std::string(tldpath / ".cmd_ran")).close();
    }
    if (!std::filesystem::exists(incpath)) {
        std::filesystem::create_directories(incpath);

        fpath p_inc_path = tldpath / bin.include_path;
        std::error_code ec;
        std::filesystem::copy(p_inc_path,incpath,std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive,ec);
        if (ec) cbu::log_warn(ec.message());
    }

    string nm;
    if (bin.dll_name) {
        nm = mcc::compiler::platform_windows.get_shared_name(bin.dll_name);
        if (!find_copy_from_dir(binpath,tldpath,nm)) {
            cbu::log_error(false,std::format("Could not find {} in external {}",nm,name));

            return 1;
        }
    } if (bin.bin_name) {
        if (pt == &mcc::compiler::platform_windows) {
            nm = mcc::compiler::platform_windows.get_import_name(bin.bin_name);
            if (!find_copy_from_dir(binpath,tldpath,nm)) {
                cbu::log_error(false,std::format("Could not find {} in external {}",nm,name));

                return 1;
            }
        } else {
            nm = pt->get_shared_name(bin.bin_name);
            if (!find_copy_from_dir(binpath,tldpath,nm)) {
                cbu::log_error(false,std::format("Could not find {} in external {}",nm,name));

                return 1;
            }
        }
    }

    return 0;
}
U8 mcc::installer::install_all(ConfigObject *cfg) {
    cbu::log_info("Installing config...");
    U8 code;

    for (auto &s : cfg->sub_projects) {
        cbu::log_info("Installing subconfig first");
        code = install_all(s);
        if (code) return code;
    }
    for (auto &ext : cfg->external_objects) {
        if (ext.linux_package) {
            code = install_linux_package(ext.linux_package);
            if (mcc::consume_interrupt()) return -1;
            if (code) return code;
        } else if (ext.linux_ext_binary) {
            code = install_binary_package(cfg,ext.name,ext.linux_ext_binary,&compiler::platform_linux);
            if (code) return code;
        }
        if (ext.win_ext_binary) {
            code = install_binary_package(cfg,ext.name,ext.win_ext_binary,&compiler::platform_windows);
            if (code) return code;
        }
    }

    cbu::log_success("Config installed succesfully");
    return 0;
}
