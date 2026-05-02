#include "ch.h"
#include "hal.h"

int main(void) {

    halInit();
    chSysInit();

    palSetLine(LINE_LED_RED);

    while (true) {
        palToggleLine(LINE_LED_GREEN);
        chThdSleepMilliseconds(100);
    }
}