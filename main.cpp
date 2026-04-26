#include "ch.h"
#include "hal.h"
#include "portab.h"

int main(void) {

    halInit();
    chSysInit();

    palSetLine(LED_RED);

    while (true) {
        palSetLine(LED_GREEN);
        chThdSleepMilliseconds(500);
        palClearLine(LED_GREEN);
        chThdSleepMilliseconds(500);
    }
}