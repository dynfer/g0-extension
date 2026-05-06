#ifndef SPI_APP_PROTOCOL_H
#define SPI_APP_PROTOCOL_H

#include <stdint.h>

static constexpr uint8_t spiAppFrameSize = 36U;
static constexpr uint8_t spiAppHeaderSize = 4U;

static constexpr uint8_t spiCmdNop = 0x00U;
static constexpr uint8_t spiCmdReadVersion = 0x01U;
static constexpr uint8_t spiCmdReadAnalog = 0x10U;
static constexpr uint8_t spiCmdReadDigital = 0x11U;
static constexpr uint8_t spiCmdReadDigitalAll = 0x12U;
static constexpr uint8_t spiCmdReadSent = 0x13U;
static constexpr uint8_t spiCmdSetOutput = 0x20U;
static constexpr uint8_t spiCmdDisableOutput = 0x21U;
static constexpr uint8_t spiCmdReadOutput = 0x22U;
static constexpr uint8_t spiCmdSetInputMode = 0x30U;
static constexpr uint8_t spiCmdEnterUpdate = 0xA5U;

static constexpr uint8_t spiStatusReady = 0x00U;
static constexpr uint8_t spiStatusUpdateMode = 0x01U;
static constexpr uint8_t spiResultOk = 0x00U;
static constexpr uint8_t spiResultInvalid = 0x01U;
static constexpr uint8_t spiResultRange = 0x02U;
static constexpr uint8_t spiResultBusy = 0x03U;

void spi_app_protocol_start(void);
bool spi_app_protocol_update_requested(void);

#endif /* SPI_APP_PROTOCOL_H */
