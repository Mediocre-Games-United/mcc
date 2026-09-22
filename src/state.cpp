#include "state.hpp"
#include "compiler.hpp"
#include <mutex>

static std::mutex state_lock;

fpath mcc::state::current_project;
mcc::config::ConfigObject *mcc::state::active_config = NULL;
mcc::compiler::BuildType mcc::state::active_build = mcc::compiler::BuildType::BUILD_NONE;
mcc::compiler::Platform mcc::state::active_platform = mcc::compiler::Platform::PLATFORM_NONE;

void mcc::state::state_safe(std::function<void()> callback) {
    state_lock.lock();
    callback();
    state_lock.unlock();
}
