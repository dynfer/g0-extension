#include "analog.h"

#include "ch.hpp"
#include "hal.h"

static chibios_rt::BinarySemaphore adcFirstHalfReadySem(/*taken =*/true);
static chibios_rt::BinarySemaphore adcSecondHalfReadySem(/*taken =*/true);

static constexpr uint8_t numChannels = analogInputCount;
static constexpr uint16_t samplesPerChannel = 1024U;
static constexpr uint16_t halfSamplesPerChannel = samplesPerChannel / 2U;
static constexpr uint16_t adcReferenceMillivolts = 3300U;
static constexpr uint16_t adcMaxCount = 4095U;

static adcsample_t adcBuffer[numChannels * samplesPerChannel];

const uint8_t analogInputChannels[analogInputCount] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 17U, 18U
};
volatile uint16_t analogVoltagesMillivolts[analogInputCount];
volatile bool analogVoltagesReady = false;

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


static void adcDoneCallback1(ADCDriver *adcp)
{
  chSysLockFromISR();

  const bool complete = adcIsBufferComplete(adcp);
  if (complete) {
    adcSecondHalfReadySem.signalI();
  }
  else {
    adcFirstHalfReadySem.signalI();
  }

  chSysUnlockFromISR();
}

static constexpr ADCConfig adccfg1 = {0U};

static constexpr ADCConversionGroup adcgrpcfg1 = {
    true,
    numChannels,
    adcDoneCallback1,
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

static void processSamples(const adcsample_t *samples)
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

static THD_WORKING_AREA(waSamplerThread, 512);
static THD_FUNCTION(SamplerThread, arg)
{
  (void)arg;
  chRegSetThreadName("Sampler");
  while (true)
  {
    adcFirstHalfReadySem.wait();
    processSamples(&adcBuffer[0]);

    adcSecondHalfReadySem.wait();
    processSamples(&adcBuffer[numChannels * halfSamplesPerChannel]);
  }
}

void startSampling(void) {
    adcStart(&ADCD1, &adccfg1);
    chThdCreateStatic(waSamplerThread, sizeof(waSamplerThread),
                    NORMALPRIO + 1, SamplerThread, NULL);
    adcStartConversion(&ADCD1, &adcgrpcfg1, adcBuffer, samplesPerChannel);
}
