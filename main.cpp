#include "ch.h"
#include "hal.h"

#include "analog.h"
#include "inputs.h"
#include "outputs.h"
#include "spi_app_protocol.h"

int main(void) {

    halInit();
    chSysInit();

    startSampling();
    inputInit();
    outputInit();
    spi_app_protocol_start();

    palSetLine(LINE_LED_RED);

    while (true) {
        palToggleLine(LINE_LED_GREEN);
        chThdSleepMilliseconds(100);
    }
}
