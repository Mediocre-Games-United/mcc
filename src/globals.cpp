#include "commands.hpp"
#include "state.hpp"

volatile size_t mcc::interrupt_count = 0;
std::mutex mcc::interrupt_mutex{};
cbu::GlobalCBUState cbu::global_state{};

void mcc::pre_command_checks() {

}
void mcc::post_command_checks() {
    mcc::state::state_safe([]() {
        if (!mcc::state::active_config) return;

        mcc::state::active_config->reload_config_from_file();
    });
}
