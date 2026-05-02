#include "ch.h"
#include "hal.h"

#include "spi_app_protocol.h"

int main(void) {

    halInit();
    chSysInit();

    spi_app_protocol_start();

    palSetLine(LINE_LED_RED);

    while (true) {
        if (spi_app_protocol_update_requested()) {
            palSetLine(LINE_LED_RED);
            palClearLine(LINE_LED_GREEN);
            chThdSleepMilliseconds(500);
        }
        else {
            palToggleLine(LINE_LED_GREEN);
            chThdSleepMilliseconds(100);
        }
    }
}
