#include "seg7.h"
#include <wiringPi.h>
#include <dlfcn.h>
#include <unistd.h>
#define BCD_A 5   // a -> gp5
#define BCD_B 6   // b -> gp6
#define BCD_C 12  // c -> gp12
#define BCD_D 13  // d -> gp13

static int bcd_pins[4] = {BCD_A, BCD_B, BCD_C, BCD_D};
static int bcd_table[10][4] = {
    {0,0,0,0}, // 0
    {1,0,0,0}, // 1
    {0,1,0,0}, // 2
    {1,1,0,0}, // 3
    {0,0,1,0}, // 4
    {1,0,1,0}, // 5
    {0,1,1,0}, // 6
    {1,1,1,0}, // 7
    {0,0,0,1}, // 8
    {1,0,0,1}  // 9
};

int seg7_display(int num) {
    wiringPiSetupGpio();
    for (int i = 0; i < 4; i++) pinMode(bcd_pins[i], OUTPUT);
    if (num < 0 || num > 9) return -1;
    for (int i = 0; i < 4; i++) digitalWrite(bcd_pins[i], bcd_table[num][i]);
    return 0;
}

int seg7_off(void) {
    wiringPiSetupGpio();
    for (int i = 0; i < 4; i++) {
        pinMode(bcd_pins[i], OUTPUT);
        digitalWrite(bcd_pins[i], 0);
    }
    return 0;
}

// 7-Segment 숫자 표시 함수: num부터 0까지 1초 간격으로 표시, 0에서 buzzer 동작
void seg7_display_full(int num) {
    int seg_pins[7] = {BCD_A, BCD_B, BCD_C, BCD_D};
    int digits[10][7] = {
        {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1},
        {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0},
        {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
    };
    wiringPiSetupGpio(); // GPIO 모드 설정
    for (int i = num; i >= 0; i--) {
        for (int j = 0; j < 7; j++) {
            pinMode(seg_pins[j], OUTPUT); // 출력 모드
            digitalWrite(seg_pins[j], digits[i][j]); // 세그먼트 출력
        }
        sleep(1); // 1초 대기
    }
    // buzzer.so 동적 호출: 0이 되면 부저 울림
    void *buzzer_lib = dlopen("./buzzer.so", RTLD_LAZY);
    if (buzzer_lib) {
        void (*buzzer_onoff)(int) = dlsym(buzzer_lib, "buzzer_onoff");
        if (buzzer_onoff) buzzer_onoff(27);
        dlclose(buzzer_lib);
    }
} 