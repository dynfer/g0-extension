#include "outputs.h"

#include "ch.h"
#include "hal.h"
#include <algorithm>

namespace {

constexpr uint32_t minFrequencyHz = 1U;
constexpr uint32_t maxFrequencyHz = 500U;

class SoftwareOutputs;

struct OutputRuntimeState {
    virtual_timer_t timer;
    bool highPhase;
    uint8_t outputIndex;
    SoftwareOutputs *owner;
};

class SoftwareOutputs {
public:
    void init()
    {
        chSysLock();
        for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
            states[outputIndex] = {0U, 0U};
            chVTObjectInit(&runtimeStates[outputIndex].timer);
            runtimeStates[outputIndex].highPhase = false;
            runtimeStates[outputIndex].outputIndex = outputIndex;
            runtimeStates[outputIndex].owner = this;
            configureOutputI(outputIndex);
        }
        chSysUnlock();
    }

    bool set(uint8_t output, uint32_t frequencyHz, uint16_t duty)
    {
        if (!isValid(output)) {
            return false;
        }

        if (duty > outputDutyMax) {
            duty = outputDutyMax;
        }

        const uint8_t outputIndex = output - 1U;
        const uint32_t requestedFrequencyHz =
            frequencyHz == 0U ? 0U : std::clamp(frequencyHz, minFrequencyHz, maxFrequencyHz);

        chSysLock();
        states[outputIndex] = {requestedFrequencyHz, duty};
        configureOutputI(outputIndex);
        chSysUnlock();

        return true;
    }

    void disable(uint8_t output)
    {
        if (!isValid(output)) {
            return;
        }

        const uint8_t outputIndex = output - 1U;

        chSysLock();
        states[outputIndex] = {0U, 0U};
        configureOutputI(outputIndex);
        chSysUnlock();
    }

    OutputState getState(uint8_t output)
    {
        if (!isValid(output)) {
            return {0U, 0U};
        }

        OutputState state;
        chSysLock();
        state = states[output - 1U];
        chSysUnlock();
        return state;
    }

private:
    OutputState states[outputCount] = {};
    OutputRuntimeState runtimeStates[outputCount] = {};

    static bool isValid(uint8_t output)
    {
        return (output >= 1U) && (output <= outputCount);
    }

    static ioline_t getLine(uint8_t outputIndex)
    {
        switch (outputIndex) {
            case 0U:
                return LINE_TIM3_CH2;
            case 1U:
                return LINE_TIM3_CH1;
            case 2U:
                return LINE_TIM1_CH2;
            default:
                return LINE_TIM1_CH1;
        }
    }

    static sysinterval_t periodTicks(uint32_t frequencyHz)
    {
        const uint32_t clampedFrequency = std::clamp(frequencyHz, minFrequencyHz, maxFrequencyHz);
        sysinterval_t ticks = static_cast<sysinterval_t>(
            (CH_CFG_ST_FREQUENCY + (clampedFrequency / 2U)) / clampedFrequency);
        return ticks == 0U ? 1U : ticks;
    }

    static sysinterval_t highTicks(const OutputState &state)
    {
        return static_cast<sysinterval_t>(
            (static_cast<uint32_t>(periodTicks(state.frequencyHz)) * state.duty) /
            outputDutyMax);
    }

    static sysinterval_t lowTicks(const OutputState &state)
    {
        return periodTicks(state.frequencyHz) - highTicks(state);
    }

    static bool hasEdges(const OutputState &state)
    {
        if (state.frequencyHz == 0U) {
            return false;
        }

        const sysinterval_t high = highTicks(state);
        const sysinterval_t low = lowTicks(state);
        return (high != 0U) && (low != 0U);
    }

    static bool staticLevel(const OutputState &state)
    {
        return state.frequencyHz == 0U ? (state.duty != 0U) : (state.duty >= outputDutyMax);
    }

    static void timerCallback(virtual_timer_t *vtp, void *par)
    {
        (void)vtp;
        OutputRuntimeState *runtime = static_cast<OutputRuntimeState *>(par);
        runtime->owner->onTimerI(runtime->outputIndex);
    }

    void onTimerI(uint8_t outputIndex)
    {
        chSysLockFromISR();
        const OutputState &state = states[outputIndex];
        OutputRuntimeState &runtime = runtimeStates[outputIndex];

        if (hasEdges(state)) {
            const ioline_t line = getLine(outputIndex);

            runtime.highPhase = !runtime.highPhase;
            palWriteLine(line, runtime.highPhase ? PAL_HIGH : PAL_LOW);

            const sysinterval_t delay =
                runtime.highPhase ? highTicks(state) : lowTicks(state);
            chVTSetI(&runtime.timer, delay, timerCallback, &runtime);
        }
        chSysUnlockFromISR();
    }

    void configureOutputI(uint8_t outputIndex)
    {
        OutputRuntimeState &runtime = runtimeStates[outputIndex];
        const OutputState &state = states[outputIndex];
        const ioline_t line = getLine(outputIndex);

        chVTResetI(&runtime.timer);
        palSetLineMode(line, PAL_MODE_OUTPUT_PUSHPULL);

        if (!hasEdges(state)) {
            runtime.highPhase = staticLevel(state);
            palWriteLine(line, runtime.highPhase ? PAL_HIGH : PAL_LOW);
            return;
        }

        runtime.highPhase = true;
        palWriteLine(line, PAL_HIGH);
        chVTSetI(&runtime.timer, highTicks(state), timerCallback, &runtime);
    }
};

SoftwareOutputs outputs;

}  // namespace

void outputInit(void)
{
    outputs.init();
}

bool outputSet(uint8_t output, uint32_t frequencyHz, uint16_t duty)
{
    auto freq = std::clamp(frequencyHz, minFrequencyHz, maxFrequencyHz);
    return outputs.set(output, freq, duty);
}

void outputDisable(uint8_t output)
{
    outputs.disable(output);
}

OutputState outputGetState(uint8_t output)
{
    return outputs.getState(output);
}
