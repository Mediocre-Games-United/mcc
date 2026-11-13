#include "state.hpp"
#include <mutex>

static std::mutex state_lock;

fpath mcc::state::current_project;
mcc::config::ConfigObject *mcc::state::active_config = NULL;
mcc::compiler::BuildType mcc::state::active_build = NULL;
mcc::compiler::Platform mcc::state::active_platform = NULL;

void mcc::state::state_safe(std::function<void()> callback) {
    state_lock.lock();
    callback();
    state_lock.unlock();
}
