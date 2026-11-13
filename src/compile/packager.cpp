#include "packager.hpp"
#include "compile/compiler.hpp"
#include "config_file.hpp"
#include "export_texts.hpp"
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

        string ext = (s.extension());
        // cbu::log_info(std::format("File {} has extension {}",(s),ext));
        if (cbu::vector_has_value(banned_extensions,ext)) {
            cbu::log_info(std::format("Skipped copying {} due to banned extension {}",(s),ext));

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
                                             (s),(tgt),ec.message()));
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
                                             (s),(tgt),ec.message()));
        }
    }
}

U8 mcc::packager::package(fpath target_path,mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version) {
    fpath res_path = target_path / "resources";
    fpath lang_path = target_path / "lang";

    fpath res_src_path = cfg->directory / "resources";
    fpath lang_src_path = cfg->directory / "lang";

    for (auto &s : cfg->sub_projects) {
        mcc::packager::package_all(s,type,pt,exp,version);
        fpath build_path = mcc::compiler::get_build_path(s,type,pt,exp,version);
        fpath rpath = build_path / "resources";
        fpath lpath = build_path / "lang";

        iterate_res_srcs_recursive(rpath,rpath,res_path);
        iterate_lang_srcs_recursive(lpath,lpath,lang_path);
    }


    iterate_res_srcs_recursive(res_src_path,res_src_path,res_path);
    iterate_lang_srcs_recursive(lang_src_path,lang_src_path,lang_path);

    return 0;
}
U8 mcc::packager::package_all(mcc::config::ConfigObject *cfg,mcc::compiler::BuildType type,mcc::compiler::Platform pt,mcc::config::ExportType exp,mcc::version::Version version) {
    fpath build_path = mcc::compiler::get_build_path(cfg,type,pt,exp,version);

    return package(build_path,cfg,type,pt,exp,version);
}

static void copy_all_packaged_recursive(fpath root,fpath cdir,fpath target) {
    for (auto &s : cbu::iterate_dir(cdir)) {
        string name = (s.filename());
        if (std::filesystem::is_directory(s)) {
            if (name == "out") continue;

            cbu::log_verbose(std::format("Recurse {}",(s)));
            copy_all_packaged_recursive(root,s,target);
            continue;
        }

        if (name.ends_with(".dll.a")) continue;
        if (name.ends_with(".o") || name.ends_with(".d")) continue;

        fpath rel = std::filesystem::relative(s,root);
        fpath tgt = target / rel;
        std::filesystem::create_directories(tgt.parent_path());

        std::error_code ec;
        std::filesystem::copy_file(s,tgt,std::filesystem::copy_options::overwrite_existing,ec);
        if (ec) {
            cbu::log_error(false,std::format("Failed to copy '{}' to '{}' with message '{}'",
                                             (s),(tgt),ec.message()));
        }
    }
}

static U8 export_headers(mcc::config::ConfigObject *cfg,mcc::compiler::Platform pt,mcc::compiler::BuildType tp,mcc::version::Version ver) {
    fpath export_path = mcc::compiler::get_export_path(cfg,&mcc::config::export_type_shared_headers,tp,pt,ver);
    fpath rpath = export_path / "build";
    fpath tpath = export_path / "export";

    string n = pt->get_shared_name(cfg->get_raw_name());
    std::error_code ec;

    std::filesystem::create_directories(tpath / "bin");
    std::filesystem::copy_file(rpath / "out" / n,tpath / "bin" / n,std::filesystem::copy_options::overwrite_existing,ec);
    if (ec) cbu::log_warn(ec.message());

    for (auto &h : cfg->header_files) {
        fpath apath = cfg->directory / cfg->src_directory / h.path;
        cbu::log_debug(std::format("Rel {}, abs {}",h.path,apath));

        fpath tgt = tpath / "include" / (h.name + ".hpp");
        std::filesystem::create_directories(tgt.parent_path());
        std::filesystem::copy_file(apath,tgt,std::filesystem::copy_options::overwrite_existing,ec);
        if (ec) cbu::log_warn(ec.message());
    }

    return 0;
}

U8 mcc::packager::export_all(mcc::config::ConfigObject *cfg) {
    if (cfg->export_types.empty()) {
        cbu::log_error(false,"Config has no export types!");

        return -1;
    }
    auto &version = cfg->version;

    for (auto &exp : cfg->export_types) {
        compiler::Platform pt = exp.platform;
        compiler::BuildType tp = exp.buildtype;

        std::error_code ec;
        auto code = mcc::compiler::build_all(cfg,tp,pt,version,exp.type);
        if (code) {
            cbu::log_error(false,"Build failed");
            return -1;
        }
        code = package_all(cfg,tp,pt,exp.type,version);
        if (code) {
            cbu::log_error(false,"Package failed");
            return -1;
        }

        fpath export_path = mcc::compiler::get_export_path(cfg,exp.type,tp,pt,version);
        fpath rpath = export_path / "build";
        fpath tpath = export_path / "export";
        cbu::log_info(std::format("Export Path: {} rpath {}",(export_path),(rpath)));
        std::filesystem::remove_all(tpath,ec);
        copy_all_packaged_recursive(rpath,rpath,tpath);
    }


    for (auto &exp : cfg->export_types) {
        compiler::Platform pt = exp.platform;
        compiler::BuildType tp = exp.buildtype;
        fpath export_path = mcc::compiler::get_export_path(cfg,exp.type,tp,pt,version);

        if (exp.type == &mcc::config::export_type_itchio) {
            string d = std::format(itch_toml_file_template);
            cbu::write_to_file_safe(export_path / "export/.itch.toml",d);
        } else if (exp.type == &mcc::config::export_type_github) {
            U8 code = cbu::run_shell_command(export_path,"tar -czf github_archive.tar.gz export/",NULL);
            if (code) {
                cbu::log_error(false,"Failed to compress package");

                return -1;
            }
        } else if (exp.type == &mcc::config::export_type_portable) {
            U8 code = cbu::run_shell_command(export_path,"tar -czf portable.tar.gz export/ && zip -r portable.zip export/",NULL);
            if (code) {
                cbu::log_error(false,"Failed to compress packages");

                return -1;
            }

            #ifdef _WIN32
            // windows link
            #else
            fpath ln = cfg->directory / "latest_portable.tar.gz";
            std::error_code ec;
            if (std::filesystem::is_symlink(ln,ec)) {
                if (ec) cbu::log_warn(ec.message());
                std::filesystem::remove(ln,ec);
                if (ec) cbu::log_warn(ec.message());
            }
            std::filesystem::create_symlink(export_path / "portable.tar.gz",ln,ec);
            if (ec) cbu::log_warn(ec.message());

            ln = cfg->directory / "latest_portable.zip";
            if (std::filesystem::is_symlink(ln,ec)) {
                if (ec) cbu::log_warn(ec.message());
                std::filesystem::remove(ln,ec);
                if (ec) cbu::log_warn(ec.message());
            }
            std::filesystem::create_symlink(export_path / "portable.zip",ln,ec);
            if (ec) cbu::log_warn(ec.message());
            #endif
        } else if (exp.type == &mcc::config::export_type_shared_headers) {
            U8 code = export_headers(cfg,pt,tp,version);
            if (code) return -1;
        }
    }

    cbu::log_success("All packages OK");

    return 0;
}
