#pragma once

#include <stdint.h>

static constexpr uint8_t outputCount = 4U;
static constexpr uint16_t outputDutyMax = 10000U;

struct OutputState {
    uint32_t frequencyHz;
    uint16_t duty;
};

void outputInit(void);
bool outputSet(uint8_t output, uint32_t frequencyHz, uint16_t duty);
void outputDisable(uint8_t output);
OutputState outputGetState(uint8_t output);
