#include "basic_commands.hpp"
#include "cli.hpp"
#include "compile/compiler.hpp"
#include "compile/packager.hpp"
#include "config/config_file.hpp"
#include "file.hpp"
#include "git_commands.hpp"
#include "installer.hpp"
#include "logger.hpp"
#include "commands.hpp"
#include "publish/publish.hpp"
#include "state.hpp"
#include "stringmath.hpp"
#include "shell.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <unordered_map>

#ifdef _WIN32
static mcc::compiler::Platform default_platform = mcc::compiler::Platform::PLATFORM_WINDOWS;
#else
static mcc::compiler::Platform default_platform = mcc::compiler::Platform::PLATFORM_LINUX;
#endif
static mcc::compiler::BuildType default_buildtype = mcc::compiler::BuildType::BUILD_EDITOR;


static U8 create_package_optimized(string tp,string pt);
static U8 run_program();
static U8 list_all_commands();
static U8 export_program();
static U8 publish_program();
static U8 clean_all();
static U8 install();

static std::unordered_map<cbu::StringName,mcc::compiler::BuildType> build_type_string_lookup;
static std::unordered_map<cbu::StringName,mcc::compiler::Platform> build_platform_string_lookup;

void mcc::basic_commands::init() {
    mcc::add_command<>("help",&list_all_commands,{},"Displays this help menu :woah:");
    mcc::add_command<>("quit",(U8(*)()) NULL,{},"Quits the program (shocker!!)");

    mcc::add_command<>("config",&mcc::config::cmd,{},"Opens the config menu for creating, editing & selecting configs.");
    mcc::add_command<>("reload",&mcc::config::reload,{},"Reload config settings & detect changes to files.");
    mcc::add_command<string,string>("package",&create_package_optimized,{"BuildType","Platform"},"Creates a ready to run & distribute package. Will skip as many steps as possible and not rebuild unless source files have changed");
    mcc::add_command<>("run",&run_program,{},"Runs the package command with editor and current platform and starts the executable.");
    mcc::add_command<>("export",&export_program,{},"Builds the program and exports in the config's export format(s) and platform(s), ready to install/execute/publish.");
    mcc::add_command<>("publish",&publish_program,{},"Exports and publishes the program in the config's export format(s), shorthand for package, export, publish");
    mcc::add_command<>("install",&install,{},"Install required dependencies");
    mcc::add_command<>("clean",&clean_all,{},"Remove all build & export files");
    mcc::add_command<>("gsync",&mcc::git::sync,{},"Safe push & pull from git remotes");
    mcc::add_command<>("gundo",&mcc::git::undo,{},"Undo local commit");
    mcc::add_command<>("gstatus",&mcc::git::status,{},"Show git status");
    mcc::add_command<string>("commit",&mcc::git::commit_all,{"CommitMessage"},"Commit all staged & unstaged changes.");
    mcc::add_command<string,string>("subcommit",&mcc::git::commit_all_sub,{"SubModule","CommitMessage"},"Commit all staged & unstaged changes in submodule.");
    mcc::add_command<string>("gswitch",&mcc::git::change_branch,{"BranchName"},"Syncs changes & changes branches safely");
    mcc::add_command<string,string>("newbranch",&mcc::git::new_branch,{"BaseBranch","NewBranch"},"Safely syncs & creates a new branch from base branch");

    cbu::log_success("Initialized basic commands");

    mcc::config::init();
    cbu::log_success("Initialized config");

    state::state_safe([]() {
        state::active_build = default_buildtype;
        state::active_platform = default_platform;
    });

    build_type_string_lookup["editor"] = mcc::compiler::BuildType::BUILD_EDITOR;
    build_type_string_lookup["e"] = mcc::compiler::BuildType::BUILD_EDITOR;
    build_type_string_lookup["beta"] = mcc::compiler::BuildType::BUILD_BETA;
    build_type_string_lookup["b"] = mcc::compiler::BuildType::BUILD_BETA;
    build_type_string_lookup["debug"] = mcc::compiler::BuildType::BUILD_DEBUG;
    build_type_string_lookup["d"] = mcc::compiler::BuildType::BUILD_DEBUG;
    build_type_string_lookup["release"] = mcc::compiler::BuildType::BUILD_RELEASE;
    build_type_string_lookup["r"] = mcc::compiler::BuildType::BUILD_RELEASE;

    build_platform_string_lookup["win"] = mcc::compiler::Platform::PLATFORM_WINDOWS;
    build_platform_string_lookup["windows"] = mcc::compiler::Platform::PLATFORM_WINDOWS;
    build_platform_string_lookup["win32"] = mcc::compiler::Platform::PLATFORM_WINDOWS;
    build_platform_string_lookup["w"] = mcc::compiler::Platform::PLATFORM_WINDOWS;

    build_platform_string_lookup["linux"] = mcc::compiler::Platform::PLATFORM_LINUX;
    build_platform_string_lookup["lin"] = mcc::compiler::Platform::PLATFORM_LINUX;
    build_platform_string_lookup["l"] = mcc::compiler::Platform::PLATFORM_LINUX;
}

static U8 list_all_commands() {
    auto commands = mcc::list_commands();
    for (auto &cmd : commands) {
        cbu::cli_output(std::format("[{}] {}\n",cmd.name,cmd.help_example));
    }

    return 0;
}

static void log_config(mcc::config::ConfigObject *obj) {
    if (!obj) cbu::log_error(true,"Active config is null!");
    cbu::log_info(std::format("Using config {}",obj->name));
}
static mcc::compiler::BuildType type = mcc::compiler::BuildType::BUILD_EDITOR;
static mcc::compiler::Platform platform = mcc::compiler::Platform::PLATFORM_LINUX;

static U8 check_config() {
    if (!mcc::config::valid()) {
        cbu::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }

    return 0;
}
static U8 create_package() {
    if (check_config()) return 1;

    U8 code;
    mcc::state::state_safe([&code]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config(mcc::state::active_config);
        code = mcc::compiler::build_all(mcc::state::active_config,type,platform,mcc::state::active_config->version,mcc::config::ExportType::EXPORT_NONE);
        if (code) return;
        code = mcc::packager::package_all(mcc::state::active_config,type,platform,mcc::config::ExportType::EXPORT_NONE);
    });

    return code;
}
static U8 create_package_optimized(string tp,string pt) {
    if (!build_type_string_lookup.contains(tp)) {
        cbu::log_error(false,std::format("Invalid BuildType '{}'",tp));

        return -1;
    }
    if (!build_platform_string_lookup.contains(pt)) {
        cbu::log_error(false,std::format("Invalid Platform '{}'",pt));

        return -1;
    }
    type = build_type_string_lookup[tp];
    platform = build_platform_string_lookup[pt];

    return create_package();
}
static U8 run_program() {
#ifdef _WIN32
    U8 res = create_package_optimized("e","w");
#else
    U8 res = create_package_optimized("e","l");
#endif
    if (res) {
        cbu::log_error(false,std::format("Failed to create package, exit code {}",res));
        return res;
    } else cbu::log_verbose("Package created, starting the run wrapper");

    fpath build_path;
    mcc::state::state_safe([&build_path]() {
        build_path = mcc::compiler::get_build_path(mcc::state::active_config,type,platform,mcc::config::ExportType::EXPORT_NONE);
    });

    cbu::log_info("Starting program...");
    U8 code = cbu::run_shell_command(build_path,std::format("{}/launcher",cbu::path_to_utf8(build_path)),NULL);
    if (code) {
        cbu::log_error(false,std::format("Program exited with code {}",code));
    } else cbu::log_success("Program exited with code 0!");

    return code;
}

static U8 export_program() {
    if (check_config()) return 1;

    U8 code;
    mcc::state::state_safe([&code]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config(mcc::state::active_config);
        code = mcc::packager::export_all(mcc::state::active_config,mcc::state::active_config->version);
    });

    return code;
}
static U8 publish_program() {
    if (check_config()) return 1;

    U8 res;
    mcc::state::state_safe([&res]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config(mcc::state::active_config);
        res = mcc::publisher::publish_all(mcc::state::active_config,mcc::state::active_config->version,true);
    });
    if (res) {
        cbu::log_error(false,"Publish failed");
        return res;
    }

    return 0;
}
static U8 install() {
    U8 code;
    mcc::state::state_safe([&code]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config(mcc::state::active_config);
        code = mcc::installer::install_all(mcc::state::active_config);
    });

    return code;
}
static U8 clean_all() {
    if (!mcc::config::valid()) {
        cbu::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }


    mcc::state::state_safe([]() {
        log_config(mcc::state::active_config);
        for (auto &s : mcc::state::active_config->sub_projects) {
            std::filesystem::remove_all(s->directory / "build");
            std::filesystem::remove_all(s->directory / "export");
            std::filesystem::remove_all(s->directory / ".mcc");
        }

        std::filesystem::remove_all(mcc::state::active_config->directory / "build");
        std::filesystem::remove_all(mcc::state::active_config->directory / "export");
        std::filesystem::remove_all(mcc::state::active_config->directory / ".mcc");
    });
    cbu::log_info("Succesfully cleaned");

    return 0;
}
