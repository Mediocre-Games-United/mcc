#include "basic_commands.hpp"
#include "cli.hpp"
#include "compile/compiler.hpp"
#include "compile/packager.hpp"
#include "config/config_file.hpp"
#include "file.hpp"
#include "logger.hpp"
#include "commands.hpp"
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


static uint8_t create_package_optimized(string tp,string pt);
static uint8_t run_program();
static uint8_t list_all_commands();
static uint8_t export_program();
static uint8_t publish_program();
static uint8_t clean_all();

static std::unordered_map<cbu::StringName,mcc::compiler::BuildType> build_type_string_lookup;
static std::unordered_map<cbu::StringName,mcc::compiler::Platform> build_platform_string_lookup;

void mcc::basic_commands::init() {
    mcc::add_command<>("help",&list_all_commands,{},"Displays this help menu :woah:");
    mcc::add_command<>("quit",(uint8_t(*)()) NULL,{},"Quits the program (shocker!!)");

    mcc::add_command<>("config",&mcc::config::cmd,{},"Opens the config menu for creating, editing & selecting configs.");
    mcc::add_command<>("reload",&mcc::config::reload,{},"Reload config settings & detect changes to files.");
    mcc::add_command<string,string>("package",&create_package_optimized,{"BuildType","Platform"},"Creates a ready to run & distribute package. Will skip as many steps as possible and not rebuild unless source files have changed");
    mcc::add_command<>("run",&run_program,{},"Runs the package command with editor and current platform and starts the executable.");
    mcc::add_command<>("export",&export_program,{},"Builds the program and exports in the config's export format(s) and platform(s), ready to install/execute/publish.");
    mcc::add_command<>("publish",&publish_program,{},"Exports and publishes the program in the config's export format(s), shorthand for package, export, publish");
    mcc::add_command<>("clean",&clean_all,{},"Remove all temporary build & export files");

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

static uint8_t list_all_commands() {
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

static uint8_t create_package() {
    if (!mcc::config::valid()) {
        cbu::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }


    uint8_t code;
    mcc::state::state_safe([&code]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config_src(mcc::state::active_config);
        code = mcc::compiler::build_all(mcc::state::active_config,type,platform);
        if (code) return;
        code = mcc::packager::package_all(mcc::state::active_config,type,platform);
    });

    return code;
}
static uint8_t create_package_optimized(string tp,string pt) {
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
static uint8_t run_program() {
#ifdef _WIN32
    uint8_t res = create_package_optimized("e","w");
#else
    uint8_t res = create_package_optimized("e","l");
#endif
    if (res) {
        cbu::log_error(false,std::format("Failed to create package, exit code {}",res));
        return res;
    } else cbu::log_verbose("Package created, starting the run wrapper");

    fpath build_path;
    mcc::state::state_safe([&build_path]() {
        build_path = mcc::compiler::get_build_path(mcc::state::active_config,type,platform);
    });

    cbu::log_info("Starting program...");
    uint8_t code = cbu::run_shell_command(build_path,std::format("{}/launcher",cbu::path_to_utf8(build_path)),NULL);
    if (code) {
        cbu::log_error(false,std::format("Program exited with code {}",code));
    } else cbu::log_success("Program exited with code 0!");

    return code;
}

static uint8_t export_program() {
    uint8_t code;
    mcc::state::state_safe([&code]() {
        log_config(mcc::state::active_config);

        mcc::config::update_config_src(mcc::state::active_config);
        code = mcc::packager::export_all(mcc::state::active_config);
    });

    return code;
}
static uint8_t publish_program() {
    uint8_t res = export_program();
    if (res) {
        cbu::log_error(false,"Export failed");
        return res;
    }

    return 0;
}
static uint8_t clean_all() {
    if (!mcc::config::valid()) {
        cbu::log_error(false,"No valid config found. Generate one with `config`");

        return 1;
    }


    mcc::state::state_safe([]() {
        log_config(mcc::state::active_config);

        std::filesystem::remove_all(mcc::state::active_config->directory / "build");
        std::filesystem::remove_all(mcc::state::active_config->directory / "export");
    });
    cbu::log_info("Succesfully cleaned");

    return 0;
}
