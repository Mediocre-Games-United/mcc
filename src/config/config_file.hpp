#pragma once
#include <cstdint>

namespace mcc {
    namespace config {
        void init();
        void background();
        uint8_t cmd();
        bool valid();
    }
}
