 Raspberry Pi 원격 GPIO 제어 시스템 (TCP)

## 프로젝트 개요
- Raspberry Pi 4를 서버(데몬)로, Ubuntu를 클라이언트로 하여 TCP로 원격에서 LED, 부저, 조도센서, 7-Segment, 버튼을 제어하는 시스템입니다.
- 각 디바이스 제어는 동적 라이브러리(.so)로 분리되어, 업그레이드/교체가 용이합니다.
- 추가기능: 버튼을 누르면 음악+LED+세그먼트 카운트다운, 중간에 다시 누르면 즉시 초기화.
- **ALL_OFF**: 모든 디바이스(LED, 부저, 세그먼트) 즉시 OFF
- **TIMER**: 예약 시간(초) 후 자동으로 ALL_OFF 실행

## 핀 배치
| 디바이스      | 핀 번호         |
|---------------|----------------|
| LED           | 17             |
| Buzzer        | 18             |
| Light Sensor  | 27             |
| 7-Segment(BCD)| 5(a), 6(b), 12(c), 13(d) |
| Push Button   | 21             |

## 빌드 방법
```sh
make
```
- 서버: `gpio_server_daemon`
- 클라이언트: `gpio_client`
- 각 디바이스 라이브러리: `led.so`, `buzzer.so`, `light_sensor.so`, `seg7.so`

## 실행 방법
### 서버 (라즈베리파이에서)
```sh
sudo ./gpio_server_daemon 
```

### 클라이언트 (우분투에서)
```sh
./gpio_client <라즈베리파이_IP>
```

## 주요 명령/기능 (클라이언트 메뉴)
1. LED ON/OFF, 밝기(최소/중간/최대)
2. BUZZER ON/OFF, 음악 재생(1=곰 세 마리, 2=아이돌 선택)
3. 7-Segment 숫자 표시/끄기
4. 조도센서 값 읽기
5. 추가기능(버튼+음악+LED+세그먼트)
6. **ALL_OFF**: 전체 OFF
7. **TIMER**: 예약 OFF (예: TIMER:10 → 10초 후 전체 OFF)

## 명령 예시
- `LED:ON` / `LED:OFF` / `LED:BRIGHT:2`
- `BUZZER:ON` / `BUZZER:OFF` / `BUZZER:MUSIC:1` (곰 세 마리) / `BUZZER:MUSIC:2` (아이돌)
- `SEG7:5` / `SEG7:OFF`
- `SENSOR:27`
- `EXTRA_MUSIC_MODE`
- `ALL_OFF`
- `TIMER:10` (10초 후 전체 OFF)

## 추가기능 동작
- 버튼을 누르면: LED ON, 음악 재생(선택된 곰 세 마리/아이돌), 세그먼트에 9~0초 카운트다운
- 0초 또는 동작 중 버튼을 다시 누르면 모두 OFF(초기화)

## 보안/이슈
- 서버는 데몬으로 동작하며, INT/TERM 신호에 안전하게 종료
- 클라이언트/서버 간 통신은 평문(TCP)으로, 네트워크 보안 필요시 SSH 터널 등 권장
- 동적 라이브러리 교체 시 서버 재시작 필요


## 클라이언트 UI 예시

```
========================================
  Raspberry Pi 원격 GPIO 제어 메뉴  
========================================
[1]  💡  LED ON
[2]  💡  LED OFF
[3]  💡  LED 밝기 설정
[4]  🔔  BUZZER ON
[5]  🔕  BUZZER OFF
[6]  🎵  BUZZER 음악 재생
[7]  7️⃣  7-Segment 숫자 표시
[8]  7️⃣  7-Segment OFF
[9]  🌞  조도센서 값 읽기
[10]  ⭐  추가기능(버튼+음악+LED+세그먼트)
[11]  🚨  전체 OFF (ALL_OFF)
[12]  ⏰  예약 OFF (TIMER)
[0]  ❌  종료
----------------------------------------
원하는 기능의 번호를 입력하세요: 

🎵 BUZZER 음악 재생 선택 시:
[BUZZER 음악 선택] 1=곰 세 마리, 2=아이돌
음악 번호를 입력하세요 (1/2): 
