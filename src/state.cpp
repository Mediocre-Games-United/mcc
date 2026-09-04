#include "state.hpp"
#include <mutex>

static std::mutex state_lock;

fpath mcc::current_project;
void mcc::state_safe(std::function<void()> callback) {
    state_lock.lock();
    callback();
    state_lock.unlock();
}
