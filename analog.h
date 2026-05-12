#pragma once

#include <stdint.h>

#include "for_fome/g0_spi_protocol.h"

static constexpr uint8_t analogInputCount = g0_spi_protocol::analogChannelCount;

void startSampling(void);

extern const uint8_t analogInputChannels[analogInputCount];
extern volatile uint16_t analogVoltagesMillivolts[analogInputCount];
extern volatile bool analogVoltagesReady;
