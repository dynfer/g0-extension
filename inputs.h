#pragma once

#include <stdint.h>

static constexpr uint8_t digitalInputCount = 4U;
static constexpr uint8_t digitalInputCaptureBufferSize = 32U;
static constexpr uint8_t sentFrameBufferSize = 4U;

enum class DigitalInputMode : uint8_t {
    Digital = 0U,
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

struct DigitalInputCapture {
    uint16_t periodUs;
    uint16_t highTimeUs;
};

struct SentFrame {
    uint32_t data;
    uint8_t status;
    uint8_t crc;
    uint8_t nibbleCount;
    bool valid;
};

extern volatile DigitalInputState digitalInputStates[digitalInputCount];
extern volatile SentFrame digitalInputSentFrames[digitalInputCount];
extern volatile bool digitalInputSentFrameReady[digitalInputCount];

void inputInit(void);
bool inputSetMode(uint8_t input, DigitalInputMode mode);
bool inputRead(uint8_t input);
DigitalInputState inputGetState(uint8_t input);
uint8_t inputReadCaptures(uint8_t input, DigitalInputCapture *captures, uint8_t maxCaptures);
uint8_t inputReadSentFrames(uint8_t input, SentFrame *frames, uint8_t maxFrames);
