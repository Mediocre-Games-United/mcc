#include "lsp_file.hpp"
#include "compiler.hpp"
#include "config/config_file.hpp"
#include <format>
#include "file.hpp"

static void get_includes_recurse(string &output,fpath dir) {
    if (!std::filesystem::is_directory(dir)) return;

    string stem = cbu::path_to_utf8(dir.stem());
    if (stem == ".git" || stem == "build" || stem == "export" || stem == ".mcc") return;
    output += std::format("  - \"-I{}\"\n",cbu::path_to_utf8(dir));

    for (auto &s : cbu::iterate_dir(dir)) {
        get_includes_recurse(output,s);
    }
}
static void generate_clangd_lsp(mcc::config::ConfigObject *cfg) {
    string cont = "CompileFlags:\n Add:\n  - \"-std=c++20\"\n  - \"-DEDITOR=1\"\n  - \"-I/usr/include\"\n";
    get_includes_recurse(cont,cfg->directory);
    auto external = cfg->external_objects;
    for (auto &s : cfg->sub_projects) {
        for (auto &e : s->external_objects) {
            external.push_back(e);
        }
    }
    for (auto &s : external) {
        if (s->linux_package) cont += std::format("  - \"-I{}\"\n",cbu::path_to_utf8(s->linux_package.include_path));
        if (s->linux_ext_binary) cont += std::format("  - \"-I{}\"\n",cbu::path_to_utf8(mcc::compiler::get_external_binary_path(cfg,s->name,mcc::compiler::Platform::PLATFORM_LINUX) / s->linux_ext_binary.include_path));
    }

    fpath cpath = cfg->directory / ".clangd";

    BYTEARRAY bin = BYTEARRAY(cont.begin(),cont.end());
    cbu::write_to_file_safe_binary(cpath,bin);
}
void mcc::lsp::generate_all_lsps_for_config(mcc::config::ConfigObject *cfg) {
    generate_clangd_lsp(cfg);
}
