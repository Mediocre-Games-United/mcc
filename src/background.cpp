#include "background.hpp"
#include "config/config_file.hpp"
#include "logger.hpp"
#include <thread>
#include <atomic>

static std::thread bg_thread;
static std::atomic<bool> running;
static size_t check_countdown;

static void thread_main() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        if (!running) break;

        if (check_countdown <= 0) {
            check_countdown = 100;
            mcc::config::background();
        }
        check_countdown -= 1;
    }
}
void mcc::start_background() {
    running = true;
    check_countdown = 0;
    bg_thread = std::thread(thread_main);
}
void mcc::end_background() {
    running = false;
    bg_thread.join();
}
