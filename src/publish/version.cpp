#include "version.hpp"
#include <format>

string mcc::version::get_version_string(Version &vr,string sep) {
    return std::format("v{}{}{}{}{}",vr.major,sep,vr.minor,sep,vr.patch);
}
