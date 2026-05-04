#include "analog.h"

#include "ch.hpp"
#include "hal.h"

const uint8_t analogInputChannels[analogInputCount] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 17U, 18U
};
volatile uint16_t analogVoltagesMillivolts[analogInputCount];
volatile bool analogVoltagesReady = false;

namespace {

THD_WORKING_AREA(analogThreadWa, 512);

class AnalogSampler {
public:
    void start()
    {
        adcStart(&ADCD1, &adcConfig);
        chThdCreateStatic(analogThreadWa, sizeof(analogThreadWa),
                          NORMALPRIO + 1, threadEntry, this);
        adcStartConversion(&ADCD1, &conversionGroup, adcBuffer, samplesPerChannel);
    }

private:
    static constexpr uint8_t numChannels = analogInputCount;
    static constexpr uint16_t samplesPerChannel = 1024U;
    static constexpr uint16_t halfSamplesPerChannel = samplesPerChannel / 2U;
    static constexpr uint16_t adcReferenceMillivolts = 3300U;
    static constexpr uint16_t adcMaxCount = 4095U;
    static constexpr uint32_t adcInputChannelMask =
        ADC_CHSELR_CHSEL_N(0U) |
        ADC_CHSELR_CHSEL_N(1U) |
        ADC_CHSELR_CHSEL_N(2U) |
        ADC_CHSELR_CHSEL_N(3U) |
        ADC_CHSELR_CHSEL_N(4U) |
        ADC_CHSELR_CHSEL_N(5U) |
        ADC_CHSELR_CHSEL_N(6U) |
        ADC_CHSELR_CHSEL_N(7U) |
        ADC_CHSELR_CHSEL_N(8U) |
        ADC_CHSELR_CHSEL_N(9U) |
        ADC_CHSELR_CHSEL_N(17U) |
        ADC_CHSELR_CHSEL_N(18U);

    chibios_rt::BinarySemaphore firstHalfReadySem{true};
    chibios_rt::BinarySemaphore secondHalfReadySem{true};
    adcsample_t adcBuffer[numChannels * samplesPerChannel] = {};

    static AnalogSampler *instance;

    static void adcDoneCallback(ADCDriver *adcp)
    {
        chSysLockFromISR();
        if (adcIsBufferComplete(adcp)) {
            instance->secondHalfReadySem.signalI();
        }
        else {
            instance->firstHalfReadySem.signalI();
        }
        chSysUnlockFromISR();
    }

    static THD_FUNCTION(threadEntry, arg)
    {
        static_cast<AnalogSampler *>(arg)->thread();
    }

    void thread()
    {
        chRegSetThreadName("Sampler");
        while (true) {
            firstHalfReadySem.wait();
            processSamples(&adcBuffer[0]);

            secondHalfReadySem.wait();
            processSamples(&adcBuffer[numChannels * halfSamplesPerChannel]);
        }
    }

    void processSamples(const adcsample_t *samples)
    {
        uint16_t voltagesMillivolts[analogInputCount];

        for (uint8_t channel = 0U; channel < numChannels; channel++) {
            uint32_t sum = 0U;

            for (uint16_t sample = 0U; sample < halfSamplesPerChannel; sample++) {
                sum += samples[(sample * numChannels) + channel];
            }

            const uint32_t average = sum / halfSamplesPerChannel;
            voltagesMillivolts[channel] =
                static_cast<uint16_t>((average * adcReferenceMillivolts) / adcMaxCount);
        }

        chSysLock();
        for (uint8_t channel = 0U; channel < numChannels; channel++) {
            analogVoltagesMillivolts[channel] = voltagesMillivolts[channel];
        }
        analogVoltagesReady = true;
        chSysUnlock();
    }

    static constexpr ADCConfig adcConfig = {0U};
    static constexpr ADCConversionGroup conversionGroup = {
        true,
        numChannels,
        adcDoneCallback,
        nullptr,
        ADC_CFGR1_CONT | ADC_CFGR1_RES_12BIT,
        ADC_TR_DISABLED,
        ADC_TR_DISABLED,
        ADC_TR_DISABLED,
        0U,
        0U,
        ADC_SMPR_SMP1_160P5 | ADC_SMPR_SMP2_160P5,
        adcInputChannelMask
    };
};

AnalogSampler sampler;
AnalogSampler *AnalogSampler::instance = &sampler;

}  // namespace

void startSampling(void)
{
    sampler.start();
}
