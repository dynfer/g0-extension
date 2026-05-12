#pragma once

#include <stdint.h>

namespace g0_spi_protocol {

static constexpr uint8_t analogChannelCount = 12U;
static constexpr uint8_t digitalInputCount = 4U;
static constexpr uint8_t outputCount = 4U;
static constexpr uint16_t outputDutyMax = 10000U;

static constexpr uint8_t appFrameSize = 36U;
static constexpr uint8_t appHeaderSize = 4U;

static constexpr uint8_t versionPayloadLength = 4U;
static constexpr uint8_t analogPayloadLength = 26U;
static constexpr uint8_t digitalPayloadLength = 17U;
static constexpr uint8_t digitalAllPayloadLength = 25U;
static constexpr uint8_t sentPayloadLength = 9U;
static constexpr uint8_t outputPayloadLength = 7U;

static constexpr uint8_t cmdNop = 0x00U;
static constexpr uint8_t cmdReadVersion = 0x01U;
static constexpr uint8_t cmdReadAnalog = 0x10U;
static constexpr uint8_t cmdReadDigital = 0x11U;
static constexpr uint8_t cmdReadDigitalAll = 0x12U;
static constexpr uint8_t cmdReadSent = 0x13U;
static constexpr uint8_t cmdSetOutput = 0x20U;
static constexpr uint8_t cmdDisableOutput = 0x21U;
static constexpr uint8_t cmdReadOutput = 0x22U;
static constexpr uint8_t cmdSetInputMode = 0x30U;
static constexpr uint8_t cmdEnterUpdate = 0xA5U;

static constexpr uint8_t statusReady = 0x00U;
static constexpr uint8_t statusUpdateMode = 0x01U;

static constexpr uint8_t resultOk = 0x00U;
static constexpr uint8_t resultInvalid = 0x01U;
static constexpr uint8_t resultRange = 0x02U;
static constexpr uint8_t resultBusy = 0x03U;

static constexpr uint8_t digitalMode = 0x00U;

} // namespace g0_spi_protocol
