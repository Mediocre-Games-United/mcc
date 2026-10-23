#include "version.hpp"
#include <format>

string mcc::version::get_version_string(Version &vr,string sep) {
    return std::format("v{}{}{}{}{}",vr.major,sep,vr.minor,sep,vr.patch);
}
string mcc::version::get_build_string(Version &vr) {
    return std::format("{}@{}",vr.git_commit,vr.build_date);
}
