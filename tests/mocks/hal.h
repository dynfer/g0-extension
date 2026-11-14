#pragma once
#include "ch.h"
using ioline_t = unsigned;
using adcsample_t = uint16_t;
using adcerror_t = unsigned;
constexpr int PAL_HIGH = 1, PAL_LOW = 0, PAL_MODE_OUTPUT_PUSHPULL = 1, PAL_MODE_INPUT = 0;
constexpr unsigned LINE_TIM3_CH2 = 0, LINE_TIM3_CH1 = 1, LINE_TIM1_CH2 = 2, LINE_TIM1_CH1 = 3;
constexpr unsigned LINE_TIM14_CH1 = 4, LINE_TIM15_CH1 = 5, LINE_TIM16_CH1 = 6, LINE_TIM17_CH1 = 7;
constexpr unsigned LINE_SPI2_NSS = 8, PAL_EVENT_MODE_BOTH_EDGES = 3;
inline bool fakeLevels[16] = {};
inline bool fakeLatch[16] = {true, true, true, true};
inline bool fakeOutput[16] = {};
inline bool fakeEvents[16] = {};
inline unsigned fakeFalls = 0, fakeStartupGlitches = 0;
inline void (*fakeCallbacks[16])(void*) = {};
inline void* fakeCallbackArgs[16] = {};
inline void palWriteLine(unsigned pin, int value) {
    fakeLatch[pin] = value;
    if (fakeOutput[pin]) {
        if (fakeLevels[pin] && !value) ++fakeFalls;
        fakeLevels[pin] = value;
    }
}
inline void palClearLine(unsigned pin) { palWriteLine(pin, 0); }
inline int palReadLine(unsigned pin) { return fakeLevels[pin]; }
inline void palSetLineMode(unsigned pin, int mode) {
    if (mode == PAL_MODE_OUTPUT_PUSHPULL && !fakeOutput[pin] && fakeLatch[pin]) ++fakeStartupGlitches;
    fakeOutput[pin] = mode == PAL_MODE_OUTPUT_PUSHPULL;
    if (fakeOutput[pin]) fakeLevels[pin] = fakeLatch[pin];
}
inline void palSetLineCallback(unsigned pin, void (*cb)(void*), void* arg) {
    fakeCallbacks[pin] = cb;
    fakeCallbackArgs[pin] = arg;
}
inline void palSetLineCallbackI(unsigned pin, void (*cb)(void*), void* arg) {
    assert(fakeLocks);
    palSetLineCallback(pin, cb, arg);
}
inline void palEnableLineEventI(unsigned pin, unsigned) { assert(fakeLocks); fakeEvents[pin] = true; }
inline void palDisableLineEventI(unsigned pin) {
    assert(fakeLocks);
    if (fakeEvents[pin]) {
        fakeCallbacks[pin] = nullptr;
        fakeCallbackArgs[pin] = nullptr;
    }
    fakeEvents[pin] = false;
}
struct Timer { uint32_t CNT = 0, SR = 0; };
struct GPTDriver;
struct GPTConfig { uint32_t frequency; void (*callback)(GPTDriver*); uint32_t cr2, dier; };
struct GPTDriver { Timer* tim; const GPTConfig* config; };
inline Timer fakeTim6;
inline GPTDriver GPTD6{&fakeTim6, nullptr};
constexpr unsigned STM32_TIM_SR_UIF = 1;
inline void gptStart(GPTDriver* driver, const GPTConfig* config) { driver->config = config; }
inline void gptStartContinuous(GPTDriver*, unsigned period) { assert(period == 65536); }
constexpr unsigned ADC_ACTIVE = 1, ADC_READY = 2;
struct ADCDriver { bool complete = false; unsigned state = ADC_READY; };
inline ADCDriver ADCD1;
struct ADCConfig { unsigned x; };
struct ADCConversionGroup {
    bool circular;
    unsigned count;
    void (*done)(ADCDriver*);
    void (*error)(ADCDriver*, adcerror_t);
    uint32_t a, b, c, d, e, f, g, h;
};
inline void adcStart(ADCDriver*, const ADCConfig*) {}
inline void adcStartConversion(ADCDriver* driver, const ADCConversionGroup*, uint16_t*, unsigned) {
    driver->state = ADC_ACTIVE;
}
inline bool adcIsBufferComplete(ADCDriver* driver) { return driver->complete; }
#define ADC_CHSELR_CHSEL_N(n) (1U << (n))
constexpr unsigned ADC_CFGR1_CONT = 0, ADC_CFGR1_RES_12BIT = 0, ADC_TR_DISABLED = 0;
constexpr unsigned ADC_SMPR_SMP1_160P5 = 0, ADC_SMPR_SMP2_160P5 = 0;
constexpr int SPI_READY = 1, SPI_ACTIVE = 2;
struct SPIDriver;
struct SPIConfig {
    bool circular, slave;
    void (*data)(SPIDriver*);
    void (*error)(SPIDriver*);
    int port;
    unsigned pad, cr1, cr2;
};
struct SPIDriver { int state = SPI_READY; size_t remaining = 36; size_t* dmarx = &remaining; };
inline SPIDriver SPID2;
inline unsigned fakeSpiResets = 0;
constexpr int GPIOB = 1;
constexpr unsigned GPIOB_SPI2_NSS = 12, SPI_CR2_DS_2 = 4, SPI_CR2_DS_1 = 2, SPI_CR2_DS_0 = 1;
inline void spiStart(SPIDriver* driver, const SPIConfig*) { driver->state = SPI_READY; }
inline void spiStop(SPIDriver* driver) { assert(driver->state == SPI_READY); }
inline void spiStartExchange(SPIDriver* driver, size_t size, const void*, void*) {
    assert(driver->state == SPI_READY);
    driver->state = SPI_ACTIVE;
    driver->remaining = size;
}
inline size_t dmaStreamGetTransactionSize(size_t* remaining) { return *remaining; }
inline void rccResetSPI2() { assert(fakeLocks); ++fakeSpiResets; }
inline void spiStopTransferI(SPIDriver* driver, void*) {
    assert(fakeLocks && fakeSpiResets);
    driver->state = SPI_READY;
    driver->remaining = 0;
}
