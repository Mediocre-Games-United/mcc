#include "packager.hpp"
#include "compile/compiler.hpp"
#include "logger.hpp"
#include <filesystem>
#include <format>
#include "file.hpp"


static void iterate_res_srcs_recursive(fpath src_dir,fpath cdir,fpath tgt_dir) {
    for (auto &s : cbu::iterate_dir(cdir)) {
        if (std::filesystem::is_directory(s)) {
            iterate_res_srcs_recursive(src_dir,s,tgt_dir);
            continue;
        }

        fpath rel = std::filesystem::relative(s,src_dir);
        fpath tgt = tgt_dir / rel;

        if (std::filesystem::exists(tgt)) {
            auto stamp = std::filesystem::last_write_time(tgt);
            auto stamp2 = std::filesystem::last_write_time(s);

            if (stamp2 < stamp) continue;
        }
        std::filesystem::create_directories(tgt.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(s,tgt,std::filesystem::copy_options::overwrite_existing,ec);
        if (ec) {
            cbu::log_error(false,std::format("Failed to copy '{}' to '{}' with message '{}'",
                                             cbu::path_to_utf8(s),cbu::path_to_utf8(tgt),ec.message()));
        }
    }
}
uint8_t mcc::packager::package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt) {
    fpath build_path = mcc::compiler::get_build_path(cfg,type,pt);
    cbu::log_info(std::format("Build path: {}",cbu::path_to_utf8(build_path)));

    fpath res_path = build_path / "resources";
    fpath lang_path = build_path / "lang";

    fpath res_src_path = cfg->directory / "resources";
    fpath lang_src_path = cfg->directory / "lang";

    for (auto &s : cfg->sub_projects) {
        package_all(s,type,pt);
        fpath build_path = mcc::compiler::get_build_path(s,type,pt);
        fpath rpath = build_path / "resources";

        iterate_res_srcs_recursive(rpath,rpath,res_path);
    }

    if (std::filesystem::exists(res_src_path)) {
        iterate_res_srcs_recursive(res_src_path,res_src_path,res_path);
    }

    return 0;
}
