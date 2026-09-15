#include "lsp_file.hpp"
#include "config/config_file.hpp"
#include <format>
#include "file.hpp"

static void get_includes_recurse(string &output,fpath dir) {
    if (!std::filesystem::is_directory(dir)) return;

    string stem = dir.stem();
    if (stem == ".git" || stem == "build" || stem == "export") return;
    output += std::format("- \"-I{}\"\n",cbu::path_to_utf8(dir));

    for (auto &s : cbu::iterate_dir(dir)) {
        get_includes_recurse(output,s);
    }
}
static void generate_clangd_lsp(mcc::config::ConfigObject *cfg) {
    string cont = "CompileFlags:\nAdd:\n- \"-std=c++20\"";
    fpath cpath = cfg->directory / ".clangd";

    BYTEARRAY bin = BYTEARRAY(cont.begin(),cont.end());
    cbu::write_to_file_safe_binary(cpath,bin);
}
void mcc::lsp::generate_all_lsps_for_config(mcc::config::ConfigObject *cfg) {
    generate_clangd_lsp(cfg);
}
