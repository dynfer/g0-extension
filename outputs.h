#pragma once

#include <stdint.h>

#include "for_fome/g0_spi_protocol.h"

static constexpr uint8_t outputCount = g0_spi_protocol::outputCount;
static constexpr uint16_t outputDutyMax = g0_spi_protocol::outputDutyMax;

struct OutputState {
    uint32_t frequencyHz;
    uint16_t duty;
};

void outputInit(void);
bool outputSet(uint8_t output, uint32_t frequencyHz, uint16_t duty);
void outputDisable(uint8_t output);
OutputState outputGetState(uint8_t output);
