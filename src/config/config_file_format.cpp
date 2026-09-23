#include "config_file.hpp"
#include "config_file_format.hpp"




cbu::BinaryFileSection *ConfigFileFormatV2::get_sections() {
    return new cbu::MainBinaryFileSection({
        new cbu::StringBinarySection([](void *obj,auto value) { // name
            cf *c = (cf*) obj;
            c->name = value;
        },[](void *obj) -> string {
            return ((cf*) obj)->name;
        }),
        new cbu::StringBinarySection([](void *obj,auto value) { // srcdir
            cf *c = (cf*) obj;
            c->src_directory = value;
        },[](void *obj) -> string {
            return cbu::path_to_utf8(((cf*) obj)->src_directory);
        }),
        new cbu::U8BinarySection([](void *obj,auto value) { // model
            cf *c = (cf*) obj;
            mcc::config::ConfigModel model;
            switch (value) {
                case 0: {
                    model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
                    break;
                }
                case 1: {
                    model = mcc::config::ConfigModel::SINGLE_SHARED;
                    break;
                }
                case 2: {
                    model = mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED;
                    break;
                }
                default: {
                    throw std::format("Invalid config model {}",value);
                }
            }

            c->model = model;
        },[](void *obj) -> uint8_t {
            return (uint8_t) ((cf*) obj)->model;
        }),
        new cbu::DataBinarySection([](void *obj,void *data) { // main src object
            auto *co = (mcc::config::SourceFileObject*) data;
            auto *c = (cf*) obj;

            if (co->name.empty()) return;
            c->main_source = new mcc::config::SourceFileObject(*co);
        },[]() -> void* {
            return new mcc::config::SourceFileObject();
        },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
            new cbu::StringBinarySection([](void *obj,auto value) { // main src path
                auto *c = (mcc::config::SourceFileObject*) obj;
                c->path = value;
            },[](void *obj) -> string {
                auto *c = (cf*) obj;
                if (c->main_source) return cbu::path_to_utf8(c->main_source->path);

                return "";
            }),
        }),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* { // source files
            auto *obj = (cf*) u;
            *len = obj->source_files.size();
            *size = sizeof(mcc::config::SourceFileObject*);

            if (*len == 0) return NULL;
            return obj->source_files.data();
        },[](void *obj) {},{
            new cbu::DataBinarySection([](void *obj, void *data) {
                auto *co = (mcc::config::SourceFileObject*) data;
                auto *c = (cf*) obj;

                c->source_files.push_back(new mcc::config::SourceFileObject(*co));
            },[]() -> void* { return new mcc::config::SourceFileObject(); },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
                new cbu::StringBinarySection([](void *obj,auto value) { // src path
                    auto *c = (mcc::config::SourceFileObject*) obj;
                    c->path = value;
                },[](void *obj) -> string {
                    return cbu::path_to_utf8((*(mcc::config::SourceFileObject**) obj)->path);
                }),
                new cbu::U8BinarySection([](void *obj,auto value) {
                    auto *c = (mcc::config::SourceFileObject*) obj;
                    c->enabled = bool(value);
                },[](void *obj) -> uint8_t {
                    return uint8_t((*(mcc::config::SourceFileObject**) obj)->enabled);
                })
            })
        },false),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* { // external objects
            auto *obj = (cf*) u;
            *len = obj->external_objects.size();
            *size = sizeof(mcc::config::ExternalObject*);

            if (*len == 0) return NULL;
            return obj->external_objects.data();
        },[](void*) {},{
            new cbu::DataBinarySection([](void *obj, void *data) {
                auto *co = (mcc::config::ExternalObject*) data;
                auto *c = (cf*) obj;

                c->external_objects.push_back(new mcc::config::ExternalObject(*co));
            },[]() -> void* { return new mcc::config::ExternalObject(); },[](void *obj) { delete (mcc::config::ExternalObject*) obj; },{
                new cbu::StringBinarySection([](void *obj,auto value) { // name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->name = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->name;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // linux package name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_package.name = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_package.name;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // linux package include path
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_package.include_path = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_package.include_path;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // linux package link name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_package.link_name = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_package.link_name;
                }),

                new cbu::StringBinarySection([](void *obj,auto value) { // linux bin download url
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_ext_binary.download_url = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_ext_binary.download_url;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // linux bin include path
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_ext_binary.include_path = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_ext_binary.include_path;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // linux bin name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->linux_ext_binary.bin_name = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->linux_ext_binary.bin_name;
                }),

                new cbu::StringBinarySection([](void *obj,auto value) { // win bin download url
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->win_ext_binary.download_url = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->win_ext_binary.download_url;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // win bin include path
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->win_ext_binary.include_path = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->win_ext_binary.include_path;
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // win bin name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->win_ext_binary.bin_name = value;
                },[](void *obj) -> string {
                    return (*(mcc::config::ExternalObject**) obj)->win_ext_binary.bin_name;
                }),
            })
        },false),
        new cbu::StringBinarySection([](void *obj,auto value) { // parent directory
            cf *c = (cf*) obj;
            string str = value;
            c->has_parent_directory = !str.empty();
            if (!str.empty()) {
                c->parent_directory = str;
            }

        },[](void *obj) -> string {
            cf *c = (cf*) obj;
            if (!c->has_parent_directory) return "";

            return cbu::path_to_utf8(c->parent_directory);
        }),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* {
            auto *obj = (cf*) u;
            *len = obj->export_types.size();
            *size = sizeof(mcc::config::ExportType);

            if (*len == 0) return NULL;
            return obj->export_types.data();
        },[](void*) {},{
            new cbu::U8BinarySection([](void *u,auto value) {
                auto *obj = (cf*) u;
                auto exp = mcc::config::ExportType(value);

                obj->export_types.push_back(exp);
            },[](void *u) -> uint8_t {
                auto *v = (mcc::config::ExportType*) u;
                return uint8_t(*v);
            })
        },false)
    });
}




cbu::BinaryFileSection *ConfigFileFormatV1::get_sections() {
    return new cbu::MainBinaryFileSection({
        new cbu::StringBinarySection([](void *obj,auto value) { // name
            cf *c = (cf*) obj;
            c->name = value;
        },[](void *obj) -> string {
            return ((cf*) obj)->name;
        }),
        new cbu::StringBinarySection([](void *obj,auto value) { // srcdir
            cf *c = (cf*) obj;
            c->src_directory = value;
        },[](void *obj) -> string {
            return cbu::path_to_utf8(((cf*) obj)->src_directory);
        }),
        new cbu::U8BinarySection([](void *obj,auto value) { // model
            cf *c = (cf*) obj;
            mcc::config::ConfigModel model;
            switch (value) {
                case 0: {
                    model = mcc::config::ConfigModel::SINGLE_EXECUTABLE;
                    break;
                }
                case 1: {
                    model = mcc::config::ConfigModel::SINGLE_SHARED;
                    break;
                }
                case 2: {
                    model = mcc::config::ConfigModel::EXECUTABLES_WITH_SHARED;
                    break;
                }
                default: {
                    throw std::format("Invalid config model {}",value);
                }
            }

            c->model = model;
        },[](void *obj) -> uint8_t {
            return (uint8_t) ((cf*) obj)->model;
        }),
        new cbu::DataBinarySection([](void *obj,void *data) { // main src object
            auto *co = (mcc::config::SourceFileObject*) data;
            auto *c = (cf*) obj;

            if (co->name.empty()) return;
            c->main_source = new mcc::config::SourceFileObject(*co);
        },[]() -> void* {
            return new mcc::config::SourceFileObject();
        },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
            new cbu::StringBinarySection([](void *obj,auto value) { // main src path
                auto *c = (mcc::config::SourceFileObject*) obj;
                c->path = value;
            },[](void *obj) -> string {
                auto *c = (cf*) obj;
                if (c->main_source) return cbu::path_to_utf8(c->main_source->path);

                return "";
            }),
        }),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* { // source files
            auto *obj = (cf*) u;
            *len = obj->source_files.size();
            *size = sizeof(mcc::config::SourceFileObject*);

            if (*len == 0) return NULL;
            return obj->source_files.data();
        },[](void *obj) {},{
            new cbu::DataBinarySection([](void *obj, void *data) {
                auto *co = (mcc::config::SourceFileObject*) data;
                auto *c = (cf*) obj;

                c->source_files.push_back(new mcc::config::SourceFileObject(*co));
            },[]() -> void* { return new mcc::config::SourceFileObject(); },[](void *obj) { delete (mcc::config::SourceFileObject*) obj; },{
                new cbu::StringBinarySection([](void *obj,auto value) { // src path
                    auto *c = (mcc::config::SourceFileObject*) obj;
                    c->path = value;
                },[](void *obj) -> string {
                    return cbu::path_to_utf8((*(mcc::config::SourceFileObject**) obj)->path);
                }),
                new cbu::U8BinarySection([](void *obj,auto value) {
                    auto *c = (mcc::config::SourceFileObject*) obj;
                    c->enabled = bool(value);
                },[](void *obj) -> uint8_t {
                    return uint8_t((*(mcc::config::SourceFileObject**) obj)->enabled);
                })
            })
        },false),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* { // external objects
            auto *obj = (cf*) u;
            *len = obj->external_objects.size();
            *size = sizeof(mcc::config::ExternalObject*);

            if (*len == 0) return NULL;
            return obj->external_objects.data();
        },[](void*) {},{
            new cbu::DataBinarySection([](void *obj, void *data) {
                auto *co = (mcc::config::ExternalObject*) data;
                auto *c = (cf*) obj;

                // c->external_objects.push_back(new mcc::config::ExternalObject(*co));
            },[]() -> void* { return new mcc::config::ExternalObject(); },[](void *obj) { delete (mcc::config::ExternalObject*) obj; },{
                new cbu::StringBinarySection([](void *obj,auto value) { // link name
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->name = value;
                },[](void *obj) -> string {
                    return cbu::path_to_utf8((*(mcc::config::ExternalObject**) obj)->name);
                }),
                new cbu::StringBinarySection([](void *obj,auto value) { // include path
                    auto *c = (mcc::config::ExternalObject*) obj;
                    c->name = value;
                },[](void *obj) -> string {
                    return cbu::path_to_utf8((*(mcc::config::ExternalObject**) obj)->name);
                })
            })
        },false),
        new cbu::StringBinarySection([](void *obj,auto value) { // parent directory
            cf *c = (cf*) obj;
            string str = value;
            c->has_parent_directory = !str.empty();
            if (!str.empty()) {
                c->parent_directory = str;
            }

        },[](void *obj) -> string {
            cf *c = (cf*) obj;
            if (!c->has_parent_directory) return "";

            return cbu::path_to_utf8(c->parent_directory);
        }),
        new cbu::RepeatingBinarySection([](void *u,size_t *size,size_t *len) -> void* {
            auto *obj = (cf*) u;
            *len = obj->export_types.size();
            *size = sizeof(mcc::config::ExportType);

            if (*len == 0) return NULL;
            return obj->export_types.data();
        },[](void*) {},{
            new cbu::U8BinarySection([](void *u,auto value) {
                auto *obj = (cf*) u;
                auto exp = mcc::config::ExportType(value);

                obj->export_types.push_back(exp);
            },[](void *u) -> uint8_t {
                auto *v = (mcc::config::ExportType*) u;
                return uint8_t(*v);
            })
        },false)
    });
}

cf *ConfigFileFormat::load_config(fpath path) {
    cf *obj = new cf();
    obj->directory = path.parent_path();
    load_file_to_buffer(path);
    load_buffer_to_object(obj);

    return obj;
}
void ConfigFileFormat::save_config(cf *obj) {
    load_object_to_buffer(obj);
    save_buffer_to_file(obj->directory / mcc::config::FNAME);
}
