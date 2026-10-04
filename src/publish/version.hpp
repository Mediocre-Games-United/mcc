#pragma once

#include "base_types.hpp"
#include <cstddef>

namespace mcc::version {
    struct Version {
        Version(size_t major,size_t minor,size_t patch) : major(major), minor(minor), patch(patch) {}
        size_t major;
        size_t minor;
        size_t patch;
    };

    string get_version_string(Version &vr,string sep);
}
