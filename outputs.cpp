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
};

constexpr OutputMap outputMap[outputCount] = {
    {&PWMD3, 1U, 0U},  // output1 = TIM3_CH2 PC7
    {&PWMD3, 0U, 0U},  // output2 = TIM3_CH1 PC6
    {&PWMD1, 1U, 1U},  // output3 = TIM1_CH2 PA9
    {&PWMD1, 0U, 1U},  // output4 = TIM1_CH1 PA8
};

OutputState outputStates[outputCount] = {};
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

bool isValidOutput(uint8_t output)
{
    return (output >= 1U) && (output <= outputCount);
}

pwmcnt_t periodForFrequency(uint32_t frequencyHz)
{
    if (frequencyHz < minFrequencyHz) {
        frequencyHz = minFrequencyHz;
    }
    else if (frequencyHz > maxFrequencyHz) {
        frequencyHz = maxFrequencyHz;
    }

    pwmcnt_t period = pwmClockHz / frequencyHz;
    if (period == 0U) {
        period = 1U;
    }

    return period;
}

pwmcnt_t widthForDuty(PWMDriver *driver, uint16_t duty)
{
    if (duty > outputDutyMax) {
        duty = outputDutyMax;
    }

    return PWM_FRACTION_TO_WIDTH(driver, outputDutyMax, duty);
}

void applyOutputI(uint8_t outputIndex)
{
    const OutputMap &mapping = outputMap[outputIndex];
    pwmEnableChannelI(mapping.driver, mapping.channel,
                      widthForDuty(mapping.driver, outputStates[outputIndex].duty));
}

void applyTimerFrequencyI(uint8_t timerIndex, uint32_t frequencyHz)
{
    PWMDriver *driver = (timerIndex == 0U) ? &PWMD3 : &PWMD1;

    timerFrequenciesHz[timerIndex] = frequencyHz;
    pwmChangePeriodI(driver, periodForFrequency(frequencyHz));

    for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
        if (outputMap[outputIndex].timerIndex == timerIndex) {
            outputStates[outputIndex].frequencyHz = frequencyHz;
            applyOutputI(outputIndex);
        }
    }
}

}  // namespace

void outputInit(void)
{
    pwmStart(&PWMD3, &pwm3Config);
    pwmStart(&PWMD1, &pwm1Config);

    chSysLock();
    for (uint8_t outputIndex = 0U; outputIndex < outputCount; outputIndex++) {
        outputStates[outputIndex] = {timerFrequenciesHz[outputMap[outputIndex].timerIndex], 0U};
        applyOutputI(outputIndex);
    }
    chSysUnlock();
}

bool outputSet(uint8_t output, uint32_t frequencyHz, uint16_t duty)
{
    if (!isValidOutput(output)) {
        return false;
    }

    if (frequencyHz < minFrequencyHz) {
        frequencyHz = minFrequencyHz;
    }
    else if (frequencyHz > maxFrequencyHz) {
        frequencyHz = maxFrequencyHz;
    }

    if (duty > outputDutyMax) {
        duty = outputDutyMax;
    }

    const uint8_t outputIndex = output - 1U;
    const uint8_t timerIndex = outputMap[outputIndex].timerIndex;

    chSysLock();
    outputStates[outputIndex].duty = duty;
    if (timerFrequenciesHz[timerIndex] != frequencyHz) {
        applyTimerFrequencyI(timerIndex, frequencyHz);
    }
    else {
        outputStates[outputIndex].frequencyHz = frequencyHz;
        applyOutputI(outputIndex);
    }
    chSysUnlock();

    return true;
}

void outputDisable(uint8_t output)
{
    if (!isValidOutput(output)) {
        return;
    }

    const uint8_t outputIndex = output - 1U;

    chSysLock();
    outputStates[outputIndex].duty = 0U;
    applyOutputI(outputIndex);
    chSysUnlock();
}

OutputState outputGetState(uint8_t output)
{
    if (!isValidOutput(output)) {
        return {0U, 0U};
    }

    OutputState state;

    chSysLock();
    state = outputStates[output - 1U];
    chSysUnlock();

    return state;
}
