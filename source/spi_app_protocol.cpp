#include "spi_app_protocol.h"

#include "ch.h"
#include "hal.h"

namespace {

constexpr uint32_t APP_VERSION = 1U;

constexpr uint8_t SPI_CMD_NOP = 0x00U;
constexpr uint8_t SPI_CMD_READ_VERSION = 0x01U;
constexpr uint8_t SPI_CMD_ENTER_UPDATE = 0xA5U;

constexpr uint8_t SPI_STATUS_READY = 0x00U;
constexpr uint8_t SPI_STATUS_UPDATE_MODE = 0x01U;
constexpr size_t SPI_FRAME_SIZE = 8U;

volatile bool update_requested = false;

const SPIConfig spi2_config = {
    false,
    true,
    nullptr,
    nullptr,
    GPIOB,
    GPIOB_SPI2_NSS,
    0U,
    SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
};

void fill_spi_response(uint8_t *txbuf, uint8_t last_command) {
    txbuf[0] = update_requested ? SPI_STATUS_UPDATE_MODE : SPI_STATUS_READY;
    txbuf[1] = static_cast<uint8_t>(APP_VERSION);
    txbuf[2] = static_cast<uint8_t>(APP_VERSION >> 8U);
    txbuf[3] = static_cast<uint8_t>(APP_VERSION >> 16U);
    txbuf[4] = static_cast<uint8_t>(APP_VERSION >> 24U);
    txbuf[5] = last_command;
    txbuf[6] = 0U;
    txbuf[7] = 0U;
}

void handle_spi_command(uint8_t command) {
    switch (command) {
    case SPI_CMD_NOP:
    case SPI_CMD_READ_VERSION:
        break;
    case SPI_CMD_ENTER_UPDATE:
        update_requested = true;
        break;
    default:
        break;
    }
}

THD_WORKING_AREA(spi_thread_wa, 256);
THD_FUNCTION(spi_thread, arg) {
    (void)arg;

    uint8_t rxbuf[SPI_FRAME_SIZE] = {};
    uint8_t txbuf[SPI_FRAME_SIZE] = {};
    uint8_t last_command = SPI_CMD_NOP;

    spiStart(&SPID2, &spi2_config);

    while (true) {
        fill_spi_response(txbuf, last_command);

        if (spiExchange(&SPID2, SPI_FRAME_SIZE, txbuf, rxbuf) == MSG_OK) {
            last_command = rxbuf[0];
            handle_spi_command(last_command);
        }
        else {
            spiStopTransfer(&SPID2, nullptr);
        }
    }
}

}  // namespace

void spi_app_protocol_start(void) {
    chThdCreateStatic(spi_thread_wa, sizeof(spi_thread_wa),
                      NORMALPRIO + 1, spi_thread, nullptr);
}

bool spi_app_protocol_update_requested(void) {
    return update_requested;
}
