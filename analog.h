#pragma once

#include <stdint.h>

static constexpr uint8_t analogInputCount = 12U;

void startSampling(void);

extern const uint8_t analogInputChannels[analogInputCount];
extern volatile uint16_t analogVoltagesMillivolts[analogInputCount];
extern volatile bool analogVoltagesReady;
