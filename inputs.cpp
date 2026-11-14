#include "inputs.h"

#include "ch.h"
#include "hal.h"
#include <algorithm>

namespace {

struct Input {
    ioline_t line;
    DigitalInputState state = {};
    bool haveRise = false;
    bool haveFall = false;
    uint32_t riseUs = 0;
    uint32_t highUs = 0;
    uint32_t periodUs = 0;
    systime_t lastRise = 0;
    systime_t lastCapture = 0;
};

Input inputs[digitalInputCount] = {
    {LINE_TIM14_CH1}, {LINE_TIM15_CH1}, {LINE_TIM16_CH1}, {LINE_TIM17_CH1}
};
uint32_t timerBase = 0;

void timerOverflow(GPTDriver *)
{
    chSysLockFromISR();
    timerBase += 0x10000U;
    chSysUnlockFromISR();
}

const GPTConfig timerConfig = {1000000U, timerOverflow, 0U, 0U};

uint32_t timestampI()
{
    uint32_t base = timerBase;
    uint32_t counter = GPTD6.tim->CNT;
    // Account for an overflow whose interrupt is pending while the kernel is locked.
    if (GPTD6.tim->SR & STM32_TIM_SR_UIF) {
        base += 0x10000U;
        counter = GPTD6.tim->CNT;
    }
    return base + counter;
}

sysinterval_t signalTimeout(const Input &input)
{
    const uint32_t timeoutMs = input.periodUs == 0U ? 2000U :
        std::clamp<uint32_t>((input.periodUs / 1000U) * 2U + 1U, 100U, 2000U);
    return TIME_MS2I(timeoutMs);
}

void edgeCallback(void *arg)
{
    auto &input = *static_cast<Input *>(arg);
    chSysLockFromISR();
    const uint32_t now = timestampI();
    if (input.state.mode == DigitalInputMode::Frequency) {
        if (palReadLine(input.line)) {
            if (input.haveRise && input.haveFall &&
                chVTTimeElapsedSinceX(input.lastRise) <= signalTimeout(input)) {
                const uint32_t period = now - input.riseUs;
                if (period > input.highUs && input.highUs != 0U) {
                    input.periodUs = period;
                    input.state.frequencyHz = 1000000U / period;
                    input.state.dutyPermille = static_cast<uint16_t>(
                        (static_cast<uint64_t>(input.highUs) * 1000U) / period);
                    input.state.periodUs = std::min<uint32_t>(period, 0xFFFFU);
                    input.state.highTimeUs = std::min<uint32_t>(input.highUs, 0xFFFFU);
                    input.state.captureCount++;
                    input.state.signalPresent = true;
                    input.lastCapture = chVTGetSystemTimeX();
                }
            }
            input.riseUs = now;
            input.lastRise = chVTGetSystemTimeX();
            input.haveRise = true;
            input.haveFall = false;
        } else if (input.haveRise && !input.haveFall) {
            input.highUs = now - input.riseUs;
            input.haveFall = true;
        }
    }
    chSysUnlockFromISR();
}

} // namespace

void inputInit(void)
{
    gptStart(&GPTD6, &timerConfig);
    gptStartContinuous(&GPTD6, 0x10000U);
    for (auto &input : inputs) {
        palSetLineMode(input.line, PAL_MODE_INPUT);
        palSetLineCallback(input.line, edgeCallback, &input);
    }
}

bool inputSetMode(uint8_t index, DigitalInputMode mode)
{
    // shortcut: SENT is rejected until a checksum-validated decoder is available.
    if (index < 1U || index > digitalInputCount ||
        (mode != DigitalInputMode::Digital && mode != DigitalInputMode::Frequency)) {
        return false;
    }
    auto &input = inputs[index - 1U];
    chSysLock();
    if (input.state.mode != mode) {
        palDisableLineEventI(input.line);
        input.state = {};
        input.state.mode = mode;
        input.haveRise = false;
        input.haveFall = false;
        input.periodUs = 0U;
        if (mode == DigitalInputMode::Frequency) {
            palSetLineCallbackI(input.line, edgeCallback, &input);
            palEnableLineEventI(input.line, PAL_EVENT_MODE_BOTH_EDGES);
        }
    }
    chSysUnlock();
    return true;
}

DigitalInputState inputGetState(uint8_t index)
{
    if (index < 1U || index > digitalInputCount) {
        return {};
    }
    auto &input = inputs[index - 1U];
    chSysLock();
    auto state = input.state;
    state.level = palReadLine(input.line) != 0U;
    if (state.mode == DigitalInputMode::Digital) {
        state.signalPresent = true;
    } else if (!state.signalPresent ||
               chVTTimeElapsedSinceX(input.lastCapture) >= signalTimeout(input)) {
        state.signalPresent = false;
        state.frequencyHz = 0U;
        state.dutyPermille = 0U;
        state.periodUs = 0U;
        state.highTimeUs = 0U;
    }
    chSysUnlock();
    return state;
}

bool inputRead(uint8_t index)
{
    return inputGetState(index).level;
}
