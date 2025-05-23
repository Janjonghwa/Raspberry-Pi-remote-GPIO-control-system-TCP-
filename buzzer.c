#include "buzzer.h"
#include <wiringPi.h>
#include <softTone.h>
#define BUZZER 18

int buzzer_on(void) {
    wiringPiSetupGpio();
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, 1);
    return 0;
}

int buzzer_off(void) {
    wiringPiSetupGpio();
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, 0);
    return 0;
}

// 곰 세 마리(대표 구절) 멜로디
// 음계: C C F F A A F | G G E E D D C
// 주파수: 262 262 349 349 440 440 349 | 392 392 330 330 294 294 262
int buzzer_play_music(int music_id) {
    wiringPiSetupGpio();
    pinMode(BUZZER, OUTPUT);
    softToneCreate(BUZZER);
    if (music_id == 2) {
        // 요아소비 '아이돌' 멜로디
        int notes[] = {659, 784, 880, 1047, 880, 784, 880, 1047, 880, 784, 880, 659, 784, 880, 1319, 988, 784, 880, 988, 1319, 1175, 1397, 1319};
        int beats[] = {125, 125, 250, 250, 250, 125, 250, 125, 125, 125, 250, 125, 125, 250, 250, 250, 125, 250, 125, 125, 125, 250, 250};
        int len = sizeof(notes)/sizeof(notes[0]);
        for (int i = 0; i < len; i++) {
            softToneWrite(BUZZER, notes[i]);
            delay(beats[i]);
            softToneWrite(BUZZER, 0);
            delay(20);
        }
    } else {
        // 곰 세 마리(대표 구절) 멜로디
        int notes[] = {262,262,349,349,440,440,349, 392,392,330,330,294,294,262};
        int beats[] = {1,1,1,1,1,1,2, 1,1,1,1,1,1,2};
        int len = sizeof(notes)/sizeof(notes[0]);
        for (int i = 0; i < len; i++) {
            softToneWrite(BUZZER, notes[i]);
            delay(350 * beats[i]);
        }
        softToneWrite(BUZZER, 0);
    }
    softToneWrite(BUZZER, 0);
    return 0;
} 