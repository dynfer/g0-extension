#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cassert>
using sysinterval_t = uint32_t;
using systime_t = uint32_t;
constexpr uint32_t CH_CFG_ST_FREQUENCY = 100000;
constexpr int NORMALPRIO = 1, MSG_OK = 0, MSG_TIMEOUT = -1;
inline uint64_t fakeNow = 0;
inline int fakeLocks = 0;
#define TIME_MS2I(ms) ((ms) * 100U)
#define THD_WORKING_AREA(name, size) unsigned char name[size]
#define THD_FUNCTION(name, arg) void name(void* arg)
struct virtual_timer_t {
    bool armed = false;
    uint64_t due = 0;
    void (*fn)(virtual_timer_t*, void*) = nullptr;
    void* arg = nullptr;
};
inline std::vector<virtual_timer_t*> fakeTimers;
inline void chSysLock() { assert(fakeLocks == 0); ++fakeLocks; }
inline void chSysUnlock() { assert(fakeLocks == 1); --fakeLocks; }
inline void chSysLockFromISR() { chSysLock(); }
inline void chSysUnlockFromISR() { chSysUnlock(); }
inline void chVTObjectInit(virtual_timer_t* timer) {
    *timer = {};
    if (std::find(fakeTimers.begin(), fakeTimers.end(), timer) == fakeTimers.end()) {
        fakeTimers.push_back(timer);
    }
}
inline void chVTResetI(virtual_timer_t* timer) { assert(fakeLocks); timer->armed = false; }
inline void chVTSetI(virtual_timer_t* timer, uint32_t delay,
                     void (*fn)(virtual_timer_t*, void*), void* arg) {
    assert(fakeLocks && delay);
    *timer = {true, fakeNow + delay, fn, arg};
}
inline uint32_t chVTGetSystemTimeX() { return fakeNow; }
inline uint32_t chVTTimeElapsedSinceX(uint32_t then) { return uint32_t(fakeNow - then); }
inline void* chThdCreateStatic(void*, size_t, int, void (*)(void*), void*) { return nullptr; }
inline void chRegSetThreadName(const char*) {}
inline void chThdSleepMilliseconds(unsigned ms) { fakeNow += TIME_MS2I(ms); }
inline void fakeAdvance(uint64_t target) {
    assert(target >= fakeNow);
    while (true) {
        virtual_timer_t* next = nullptr;
        for (auto* timer : fakeTimers) {
            if (timer->armed && timer->due <= target && (!next || timer->due < next->due)) {
                next = timer;
            }
        }
        if (!next) break;
        fakeNow = next->due;
        next->armed = false;
        next->fn(next, next->arg);
        assert(fakeLocks == 0);
    }
    fakeNow = target;
}
