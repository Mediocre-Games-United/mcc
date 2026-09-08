#include "state.hpp"
#include <mutex>

static std::mutex state_lock;

fpath mcc::state::current_project;
mcc::config::ConfigObject *current_config = NULL;
void mcc::state::state_safe(std::function<void()> callback) {
    state_lock.lock();
    callback();
    state_lock.unlock();
}
