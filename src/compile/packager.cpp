#include "packager.hpp"
#include "compile/compiler.hpp"
#include "config_file.hpp"
#include "logger.hpp"
#include <filesystem>
#include <format>
#include "file.hpp"
#include "shell.hpp"
#include "vectormath.hpp"
#include "version.hpp"

static vector<string> banned_extensions = {".flp"};


static void iterate_res_srcs_recursive(fpath src_dir,fpath cdir,fpath tgt_dir) {
    for (auto &s : cbu::iterate_dir(cdir)) {
        if (std::filesystem::is_directory(s)) {
            iterate_res_srcs_recursive(src_dir,s,tgt_dir);
            continue;
        }

        string ext = cbu::path_to_utf8(s.extension());
        // cbu::log_info(std::format("File {} has extension {}",cbu::path_to_utf8(s),ext));
        if (cbu::vector_has_value(banned_extensions,ext)) {
            cbu::log_info(std::format("Skipped copying {} due to banned extension {}",cbu::path_to_utf8(s),ext));

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
static void iterate_lang_srcs_recursive(fpath src_dir,fpath cdir,fpath tgt_dir) {
    for (auto &s : cbu::iterate_dir(cdir)) {
        if (std::filesystem::is_directory(s)) {
            iterate_lang_srcs_recursive(src_dir,s,tgt_dir);
            continue;
        }

        fpath rel = std::filesystem::relative(s,src_dir);
        fpath tgt = tgt_dir / rel;


        std::filesystem::create_directories(tgt.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(s,tgt,std::filesystem::copy_options::overwrite_existing,ec);
        if (ec) {
            cbu::log_error(false,std::format("Failed to copy '{}' to '{}' with message '{}'",
                                             cbu::path_to_utf8(s),cbu::path_to_utf8(tgt),ec.message()));
        } else cbu::log_success(std::format("Copied '{}' to '{}'",
            cbu::path_to_utf8(s),cbu::path_to_utf8(tgt)));
    }
}
U8 mcc::packager::package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt,mcc::config::ExportType exp) {
    fpath build_path = mcc::compiler::get_build_path(cfg,type,pt,exp);

    cbu::log_info(std::format("Build path: {}",cbu::path_to_utf8(build_path)));

    fpath res_path = build_path / "resources";
    fpath lang_path = build_path / "lang";

    fpath res_src_path = cfg->directory / "resources";
    fpath lang_src_path = cfg->directory / "lang";

    for (auto &s : cfg->sub_projects) {
        mcc::packager::package_all(s,type,pt,exp);
        fpath build_path = mcc::compiler::get_build_path(s,type,pt,exp);
        fpath rpath = build_path / "resources";
        fpath lpath = build_path / "lang";

        // if (std::filesystem::exists(rpath)) {
            iterate_res_srcs_recursive(rpath,rpath,res_path);
        // } if (std::filesystem::exists(lpath)) {
            iterate_lang_srcs_recursive(lpath,lpath,lang_path);
        // }
    }


    // if (std::filesystem::exists(res_src_path)) {
        iterate_res_srcs_recursive(res_src_path,res_src_path,res_path);
    // } if (std::filesystem::exists(lang_src_path)) {
        iterate_lang_srcs_recursive(lang_src_path,lang_src_path,lang_path);
    // }


    return 0;
}

static vector<mcc::compiler::Platform> export_platforms = {mcc::compiler::Platform::PLATFORM_LINUX,mcc::compiler::Platform::PLATFORM_WINDOWS};
static vector<mcc::compiler::BuildType> export_types = {mcc::compiler::BuildType::BUILD_BETA,mcc::compiler::BuildType::BUILD_RELEASE};
U8 mcc::packager::export_all(mcc::config::ConfigObject *cfg,mcc::version::Version version) {
    if (cfg->export_types.empty()) {
        cbu::log_error(false,"Config has no export types!");

        return -1;
    }

    for (auto &exp : cfg->export_types) {
        for (auto &pt : export_platforms) {
            for (auto &tp : export_types) {
                fpath export_path = mcc::compiler::get_export_path(cfg,exp,tp,pt);

                std::error_code ec;
                // std::filesystem::remove_all(export_path,ec);
                auto code = mcc::compiler::build_all(cfg,tp,pt,version,exp);
                if (code) {
                    cbu::log_error(false,"Build failed");
                    return -1;
                }
                code = package_all(cfg,tp,pt,exp);
                if (code) {
                    cbu::log_error(false,"Package failed");
                    return -1;
                }

                // std::filesystem::remove_all(export_path / "obj",ec);
            }
        }
    }

    cbu::log_success("All packages OK");

    for (auto &exp : cfg->export_types) {
        for (auto &pt : export_platforms) {
            for (auto &tp : export_types) {
                fpath export_path = mcc::compiler::get_export_path(cfg,exp,tp,pt);
                fpath ppath = export_path.parent_path();
                string name = std::format("{}_{}_{}_{}.zip",cfg->name,mcc::compiler::platform_names[int(pt)],mcc::compiler::build_type_names[int(tp)],mcc::version::get_version_string(version,"-"));

                switch (exp) {
                    case mcc::config::ExportType::EXPORT_ITCHIO: {
                        break;
                    }
                    default: {
                        // cbu::run_shell_command(ppath,std::format("zip -r {} {}/",name,cbu::path_to_utf8(export_path)),NULL);
                        break;
                    }
                }
            }
        }
    }

    return 0;
}
