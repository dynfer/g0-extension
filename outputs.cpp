#include "outputs.h"

#include "ch.h"
#include "hal.h"

namespace {

constexpr uint32_t pwmClockHz = 1000000U;
constexpr uint32_t defaultFrequencyHz = 1000U;
constexpr uint32_t minFrequencyHz = 1U;
constexpr uint32_t maxFrequencyHz = pwmClockHz;
constexpr uint8_t timerCount = 2U;
constexpr gptfreq_t softwarePwmClockHz = 62500U;
constexpr gptcnt_t softwarePwmMaxIntervalTicks =
    static_cast<gptcnt_t>(softwarePwmClockHz / minFrequencyHz);

struct OutputMap {
    PWMDriver *driver;
    pwmchannel_t channel;
    uint8_t timerIndex;
    iomode_t pwmMode;
};

enum class OutputDriveMode : uint8_t {
    Static,
    HardwarePwm,
    SoftwarePwm
};

struct OutputRuntimeState {
    OutputDriveMode mode;
    bool softwareHighPhase;
    gptcnt_t nextEdgeTicks;
};

class PwmOutputs {
public:
    void init()
    {
        instance = this;
        pwmStart(&PWMD3, &pwm3Config);
        pwmStart(&PWMD1, &pwm1Config);
        gptStart(&GPTD6, &softwareTimerConfig);
        gptStart(&GPTD7, &softwareTimerConfigTim3);

        chSysLock();
        for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
            states[outputIndex] = {0U, 0U};
            runtimeStates[outputIndex] = {OutputDriveMode::Static, false, 0U};
        }

        for (uint8_t timerIndex = 0U; timerIndex < timerCount; timerIndex++) {
            reconfigureTimerI(timerIndex);
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
        const uint8_t timerIndex = getMap(outputIndex).timerIndex;
        const uint32_t requestedFrequencyHz =
            frequencyHz == 0U ? 0U : clampFrequency(frequencyHz);

        chSysLock();
        states[outputIndex] = {requestedFrequencyHz, duty};
        reconfigureTimerI(timerIndex);
        chSysUnlock();

        return true;
    }

    void disable(uint8_t output)
    {
        if (!isValid(output)) {
            return;
        }

        const uint8_t outputIndex = output - 1U;
        const uint8_t timerIndex = getMap(outputIndex).timerIndex;

        chSysLock();
        states[outputIndex] = {0U, 0U};
        reconfigureTimerI(timerIndex);
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
    static PwmOutputs *instance;

    OutputState states[outputCount] = {};
    OutputRuntimeState runtimeStates[outputCount] = {};
    uint32_t hardwareTimerFrequenciesHz[timerCount] = {defaultFrequencyHz, defaultFrequencyHz};
    gptcnt_t scheduledSoftwareTicks[timerCount] = {0U, 0U};

    PWMConfig pwm3Config = {
        pwmClockHz,
        pwmClockHz / defaultFrequencyHz,
        nullptr,
        {
            {PWM_OUTPUT_ACTIVE_HIGH, nullptr},
            {PWM_OUTPUT_ACTIVE_HIGH, nullptr},
            {PWM_OUTPUT_DISABLED, nullptr},
            {PWM_OUTPUT_DISABLED, nullptr}
        },
        0U,
        0U,
        0U
    };

    PWMConfig pwm1Config = {
        pwmClockHz,
        pwmClockHz / defaultFrequencyHz,
        nullptr,
        {
            {PWM_OUTPUT_ACTIVE_HIGH, nullptr},
            {PWM_OUTPUT_ACTIVE_HIGH, nullptr},
            {PWM_OUTPUT_DISABLED, nullptr},
            {PWM_OUTPUT_DISABLED, nullptr}
        },
        0U,
        0U,
        0U
    };

    const GPTConfig softwareTimerConfig = {
        softwarePwmClockHz,
        softwareTimerCallbackTim1,
        0U,
        0U
    };

    const GPTConfig softwareTimerConfigTim3 = {
        softwarePwmClockHz,
        softwareTimerCallbackTim3,
        0U,
        0U
    };

    static bool isValid(uint8_t output)
    {
        return (output >= 1U) && (output <= outputCount);
    }

    static uint32_t clampFrequency(uint32_t frequencyHz)
    {
        if (frequencyHz < minFrequencyHz) {
            return minFrequencyHz;
        }
        if (frequencyHz > maxFrequencyHz) {
            return maxFrequencyHz;
        }
        return frequencyHz;
    }

    static pwmcnt_t periodForFrequency(uint32_t frequencyHz)
    {
        pwmcnt_t period = pwmClockHz / clampFrequency(frequencyHz);
        return period == 0U ? 1U : period;
    }

    static gptcnt_t softwarePeriodTicks(uint32_t frequencyHz)
    {
        const uint32_t clampedFrequency = clampFrequency(frequencyHz);
        uint32_t periodTicks =
            (softwarePwmClockHz + (clampedFrequency / 2U)) / clampedFrequency;

        if (periodTicks == 0U) {
            periodTicks = 1U;
        }
        if (periodTicks > softwarePwmMaxIntervalTicks) {
            periodTicks = softwarePwmMaxIntervalTicks;
        }

        return static_cast<gptcnt_t>(periodTicks);
    }

    static gptcnt_t softwareHighTicks(const OutputState &state)
    {
        return static_cast<gptcnt_t>(
            (static_cast<uint32_t>(softwarePeriodTicks(state.frequencyHz)) * state.duty) /
            outputDutyMax);
    }

    static gptcnt_t softwareLowTicks(const OutputState &state)
    {
        return static_cast<gptcnt_t>(softwarePeriodTicks(state.frequencyHz) -
                                     softwareHighTicks(state));
    }

    static bool softwareOutputHasEdges(const OutputState &state)
    {
        if (state.frequencyHz == 0U) {
            return false;
        }

        const gptcnt_t highTicks = softwareHighTicks(state);
        const gptcnt_t lowTicks = softwareLowTicks(state);
        return (highTicks != 0U) && (lowTicks != 0U);
    }

    static pwmcnt_t widthForDuty(PWMDriver *driver, uint16_t duty)
    {
        if (duty > outputDutyMax) {
            duty = outputDutyMax;
        }

        return PWM_FRACTION_TO_WIDTH(driver, outputDutyMax, duty);
    }

    static OutputMap getMap(uint8_t outputIndex)
    {
        switch (outputIndex) {
            case 0U:
                return {&PWMD3, 1U, 0U, PAL_MODE_ALTERNATE(1) | PAL_STM32_OSPEED_HIGHEST};  // output1 = TIM3_CH2 PC7
            case 1U:
                return {&PWMD3, 0U, 0U, PAL_MODE_ALTERNATE(1) | PAL_STM32_OSPEED_HIGHEST};  // output2 = TIM3_CH1 PC6
            case 2U:
                return {&PWMD1, 1U, 1U, PAL_MODE_ALTERNATE(2) | PAL_STM32_OSPEED_HIGHEST};  // output3 = TIM1_CH2 PA9
            default:
                return {&PWMD1, 0U, 1U, PAL_MODE_ALTERNATE(2) | PAL_STM32_OSPEED_HIGHEST};  // output4 = TIM1_CH1 PA8
        }
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

    static uint8_t firstOutputForTimer(uint8_t timerIndex)
    {
        return timerIndex == 0U ? 0U : 2U;
    }

    static uint8_t secondOutputForTimer(uint8_t timerIndex)
    {
        return firstOutputForTimer(timerIndex) + 1U;
    }

    static PWMDriver *driverForTimer(uint8_t timerIndex)
    {
        return timerIndex == 0U ? &PWMD3 : &PWMD1;
    }

    static GPTDriver *softwareDriverForTimer(uint8_t timerIndex)
    {
        return timerIndex == 0U ? &GPTD7 : &GPTD6;
    }

    static void softwareTimerCallbackTim3(GPTDriver *gptp)
    {
        (void)gptp;
        if (instance != nullptr) {
            instance->onSoftwareTimerI(0U);
        }
    }

    static void softwareTimerCallbackTim1(GPTDriver *gptp)
    {
        (void)gptp;
        if (instance != nullptr) {
            instance->onSoftwareTimerI(1U);
        }
    }

    void onSoftwareTimerI(uint8_t timerIndex)
    {
        const gptcnt_t elapsedTicks = scheduledSoftwareTicks[timerIndex];
        scheduledSoftwareTicks[timerIndex] = 0U;

        if (elapsedTicks == 0U) {
            return;
        }

        for (uint8_t outputIndex = firstOutputForTimer(timerIndex);
             outputIndex <= secondOutputForTimer(timerIndex);
             outputIndex++) {
            OutputRuntimeState &runtime = runtimeStates[outputIndex];
            if (runtime.mode != OutputDriveMode::SoftwarePwm) {
                continue;
            }

            if (!softwareOutputHasEdges(states[outputIndex])) {
                continue;
            }

            runtime.nextEdgeTicks = static_cast<gptcnt_t>(runtime.nextEdgeTicks - elapsedTicks);
            if (runtime.nextEdgeTicks == 0U) {
                toggleSoftwareOutputI(outputIndex);
            }
        }

        scheduleSoftwareTimerI(timerIndex);
    }

    void toggleSoftwareOutputI(uint8_t outputIndex)
    {
        const OutputState &state = states[outputIndex];
        OutputRuntimeState &runtime = runtimeStates[outputIndex];
        const ioline_t line = getLine(outputIndex);

        runtime.softwareHighPhase = !runtime.softwareHighPhase;
        palWriteLine(line, runtime.softwareHighPhase ? PAL_HIGH : PAL_LOW);
        runtime.nextEdgeTicks =
            runtime.softwareHighPhase ? softwareHighTicks(state) : softwareLowTicks(state);
    }

    void scheduleSoftwareTimerI(uint8_t timerIndex)
    {
        gptStopTimerI(softwareDriverForTimer(timerIndex));
        scheduledSoftwareTicks[timerIndex] = 0U;

        gptcnt_t nextTicks = 0U;
        bool found = false;

        for (uint8_t outputIndex = firstOutputForTimer(timerIndex);
             outputIndex <= secondOutputForTimer(timerIndex);
             outputIndex++) {
            const OutputRuntimeState &runtime = runtimeStates[outputIndex];
            if (runtime.mode != OutputDriveMode::SoftwarePwm) {
                continue;
            }

            if (!softwareOutputHasEdges(states[outputIndex])) {
                continue;
            }

            if (!found || (runtime.nextEdgeTicks < nextTicks)) {
                nextTicks = runtime.nextEdgeTicks;
                found = true;
            }
        }

        if (found) {
            scheduledSoftwareTicks[timerIndex] = nextTicks;
            gptStartOneShotI(softwareDriverForTimer(timerIndex), nextTicks);
        }
    }

    void applyOutputModeI(uint8_t outputIndex, OutputDriveMode mode)
    {
        const OutputMap mapping = getMap(outputIndex);
        const OutputState &state = states[outputIndex];
        const ioline_t line = getLine(outputIndex);
        OutputRuntimeState &runtime = runtimeStates[outputIndex];

        runtime.mode = mode;
        runtime.nextEdgeTicks = 0U;
        pwmDisableChannelI(mapping.driver, mapping.channel);

        if (mode == OutputDriveMode::Static) {
            palSetLineMode(line, PAL_MODE_OUTPUT_PUSHPULL);
            palWriteLine(line, state.duty != 0U ? PAL_HIGH : PAL_LOW);
            runtime.softwareHighPhase = state.duty != 0U;
            return;
        }

        if (mode == OutputDriveMode::HardwarePwm) {
            palSetLineMode(line, mapping.pwmMode);
            pwmEnableChannelI(mapping.driver, mapping.channel,
                              widthForDuty(mapping.driver, state.duty));
            runtime.softwareHighPhase = false;
            return;
        }

        palSetLineMode(line, PAL_MODE_OUTPUT_PUSHPULL);
        if (softwareOutputHasEdges(state)) {
            runtime.softwareHighPhase = true;
            palWriteLine(line, PAL_HIGH);
            runtime.nextEdgeTicks = softwareHighTicks(state);
        }
        else {
            const bool high = state.duty >= outputDutyMax;
            runtime.softwareHighPhase = high;
            palWriteLine(line, high ? PAL_HIGH : PAL_LOW);
        }
    }

    void reconfigureTimerI(uint8_t timerIndex)
    {
        const uint8_t firstOutput = firstOutputForTimer(timerIndex);
        const uint8_t secondOutput = secondOutputForTimer(timerIndex);
        const OutputState &firstState = states[firstOutput];
        const OutputState &secondState = states[secondOutput];

        OutputDriveMode firstMode =
            firstState.frequencyHz == 0U ? OutputDriveMode::Static : OutputDriveMode::HardwarePwm;
        OutputDriveMode secondMode =
            secondState.frequencyHz == 0U ? OutputDriveMode::Static : OutputDriveMode::HardwarePwm;

        uint32_t hardwareFrequencyHz = 0U;

        if ((firstState.frequencyHz != 0U) && (secondState.frequencyHz != 0U)) {
            if (firstState.frequencyHz == secondState.frequencyHz) {
                hardwareFrequencyHz = firstState.frequencyHz;
            }
            else if (firstState.frequencyHz > secondState.frequencyHz) {
                // One TIM cannot generate two base frequencies, so the slower
                // sibling falls back to GPIO-driven software PWM.
                secondMode = OutputDriveMode::SoftwarePwm;
                hardwareFrequencyHz = firstState.frequencyHz;
            }
            else {
                firstMode = OutputDriveMode::SoftwarePwm;
                hardwareFrequencyHz = secondState.frequencyHz;
            }
        }
        else if (firstState.frequencyHz != 0U) {
            hardwareFrequencyHz = firstState.frequencyHz;
        }
        else if (secondState.frequencyHz != 0U) {
            hardwareFrequencyHz = secondState.frequencyHz;
        }

        if ((hardwareFrequencyHz != 0U) &&
            (hardwareTimerFrequenciesHz[timerIndex] != hardwareFrequencyHz)) {
            hardwareTimerFrequenciesHz[timerIndex] = hardwareFrequencyHz;
            pwmChangePeriodI(driverForTimer(timerIndex), periodForFrequency(hardwareFrequencyHz));
        }

        applyOutputModeI(firstOutput, firstMode);
        applyOutputModeI(secondOutput, secondMode);
        scheduleSoftwareTimerI(timerIndex);
    }
};

PwmOutputs *PwmOutputs::instance = nullptr;
PwmOutputs outputs;

}  // namespace

void outputInit(void)
{
    outputs.init();
}

bool outputSet(uint8_t output, uint32_t frequencyHz, uint16_t duty)
{
    return outputs.set(output, frequencyHz, duty);
}

void outputDisable(uint8_t output)
{
    outputs.disable(output);
}

OutputState outputGetState(uint8_t output)
{
    return outputs.getState(output);
}
