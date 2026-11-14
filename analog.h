#pragma once

#include <stdint.h>

#include "for_fome/g0_spi_protocol.h"

static constexpr uint8_t analogInputCount = g0_spi_protocol::analogChannelCount;

void startSampling(void);

bool analogGetSnapshot(uint16_t *millivolts);
