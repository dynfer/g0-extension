#include "outputs.h"

#include "ch.h"
#include "hal.h"

namespace {

constexpr uint32_t pwmClockHz = 1000000U;
constexpr uint32_t defaultFrequencyHz = 1000U;
constexpr uint32_t minFrequencyHz = 1U;
constexpr uint32_t maxFrequencyHz = pwmClockHz;

struct OutputMap {
    PWMDriver *driver;
    pwmchannel_t channel;
    uint8_t timerIndex;
    iomode_t pwmMode;
};

class PwmOutputs {
public:
    void init()
    {
        pwmStart(&PWMD3, &pwm3Config);
        pwmStart(&PWMD1, &pwm1Config);

        chSysLock();
        for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
            states[outputIndex] = {0U, 0U};
            applyOutputI(outputIndex);
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
        const uint32_t requestedFrequencyHz = frequencyHz == 0U ? 0U : clampFrequency(frequencyHz);

        chSysLock();
        states[outputIndex] = {requestedFrequencyHz, duty};
        if ((requestedFrequencyHz != 0U) && (timerFrequenciesHz[timerIndex] != requestedFrequencyHz)) {
            applyTimerFrequencyI(timerIndex, requestedFrequencyHz);
        }
        else {
            applyOutputI(outputIndex);
        }
        chSysUnlock();

        return true;
    }

    void disable(uint8_t output)
    {
        if (!isValid(output)) {
            return;
        }

        chSysLock();
        states[output - 1U].frequencyHz = 0U;
        states[output - 1U].duty = 0U;
        applyOutputI(output - 1U);
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

    OutputState states[outputCount] = {};
    uint32_t timerFrequenciesHz[2] = {defaultFrequencyHz, defaultFrequencyHz};

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

    static pwmcnt_t widthForDuty(PWMDriver *driver, uint16_t duty)
    {
        if (duty > outputDutyMax) {
            duty = outputDutyMax;
        }

        return PWM_FRACTION_TO_WIDTH(driver, outputDutyMax, duty);
    }

    void applyOutputI(uint8_t outputIndex)
    {
        const OutputMap mapping = getMap(outputIndex);
        const OutputState &state = states[outputIndex];
        const ioline_t line = getLine(outputIndex);

        if (state.frequencyHz == 0U) {
            palSetLineMode(line, PAL_MODE_OUTPUT_PUSHPULL);
            palWriteLine(line, state.duty != 0U ? PAL_HIGH : PAL_LOW);
            return;
        }

        palSetLineMode(line, mapping.pwmMode);
        pwmEnableChannelI(mapping.driver, mapping.channel,
                          widthForDuty(mapping.driver, state.duty));
    }

    void applyTimerFrequencyI(uint8_t timerIndex, uint32_t frequencyHz)
    {
        PWMDriver *driver = (timerIndex == 0U) ? &PWMD3 : &PWMD1;

        timerFrequenciesHz[timerIndex] = frequencyHz;
        pwmChangePeriodI(driver, periodForFrequency(frequencyHz));

        for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
            if (getMap(outputIndex).timerIndex == timerIndex) {
                if (states[outputIndex].frequencyHz != 0U) {
                    states[outputIndex].frequencyHz = frequencyHz;
                }
                applyOutputI(outputIndex);
            }
        }
    }
};

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
