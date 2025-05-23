#include "pushbutton.h"
#include <wiringPi.h>
#define BUTTON 21

int pushbutton_read(void) {
    wiringPiSetupGpio();
    pinMode(BUTTON, INPUT);
    return digitalRead(BUTTON);
} 