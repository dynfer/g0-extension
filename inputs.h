#pragma once

#include <stdint.h>

#include "for_fome/g0_spi_protocol.h"

static constexpr uint8_t digitalInputCount = g0_spi_protocol::digitalInputCount;

enum class DigitalInputMode : uint8_t {
    Digital = g0_spi_protocol::digitalMode,
    Frequency = 1U,
    Sent = 2U,
};

struct DigitalInputState {
    DigitalInputMode mode;
    bool level;
    bool signalPresent;
    uint32_t frequencyHz;
    uint16_t dutyPermille;
    uint16_t periodUs;
    uint16_t highTimeUs;
    uint32_t captureCount;
};

void inputInit(void);
bool inputSetMode(uint8_t input, DigitalInputMode mode);
bool inputRead(uint8_t input);
DigitalInputState inputGetState(uint8_t input);
