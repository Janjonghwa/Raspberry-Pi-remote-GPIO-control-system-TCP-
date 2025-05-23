#include "light_sensor.h"
#include <wiringPi.h>
#define LIGHT_SENSOR_PIN 27
// 조도센서 값 읽기 함수: 지정 핀의 입력값 반환
int light_sensor_read(void) {
    wiringPiSetupGpio(); // GPIO 모드 설정
    pinMode(LIGHT_SENSOR_PIN, INPUT); // 입력 모드
    return digitalRead(LIGHT_SENSOR_PIN); // 값 반환
} 