#include "led.h"
#include <wiringPi.h>
#define LED 17

int led_on(void) {
    wiringPiSetupGpio();
    pinMode(LED, OUTPUT);
    digitalWrite(LED, 1);
    return 0;
}

int led_off(void) {
    wiringPiSetupGpio();
    pinMode(LED, OUTPUT);
    digitalWrite(LED, 0);
    return 0;
}

int led_set_brightness(int level) {
    wiringPiSetupGpio();
    pinMode(LED, PWM_OUTPUT);
    int pwm_val = 0;
    if (level == 0) pwm_val = 85;   // min
    else if (level == 1) pwm_val = 170; // mid
    else pwm_val = 255; // max
    pwmWrite(LED, pwm_val);
    return 0;
} 