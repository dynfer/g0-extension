#pragma once
#include "ch.h"
namespace chibios_rt {
struct BinarySemaphore {
    bool available;
    explicit BinarySemaphore(bool taken) : available(!taken) {}
    void reset(bool taken) { available = !taken; }
    int wait(uint32_t = 0) {
        bool ready = available;
        available = false;
        return ready ? MSG_OK : MSG_TIMEOUT;
    }
    void signalI() { assert(fakeLocks); available = true; }
};
}
