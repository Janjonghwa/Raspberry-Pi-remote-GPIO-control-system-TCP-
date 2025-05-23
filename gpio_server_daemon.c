// --- gpio_server_daemon.c ---
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <wiringPi.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include "led.h"
#include "buzzer.h"
#include "seg7.h"
#include "pushbutton.h"

#define SERVER_PORT 5000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define BUTTON_PIN 18
#define BACKLOG 10

// GPIO 핀 정의
#define LED_PIN 17
#define BUZZER_PIN 27
#define SENSOR_PIN 19
#define SEG_A_PIN 5   // a -> gp5
#define SEG_B_PIN 6   // b -> gp6
#define SEG_C_PIN 12  // c -> gp12
#define SEG_D_PIN 13  // d -> gp13

// 클라이언트 소켓 배열
int client_sockets[MAX_CLIENTS];

// 각 디바이스 제어용 동적 라이브러리 핸들 및 함수 포인터 선언
void *led_lib = NULL, *buzzer_lib = NULL, *sensor_lib = NULL, *seg7_lib = NULL;

// 7-세그먼트 숫자 0-9 패턴
const int seven_seg_digits[10][7] = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

// 추가기능 플래그
volatile int music_mode_active = 0;
pthread_mutex_t music_mode_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile unsigned long last_button_time = 0;

void* music_mode_thread(void* arg) {
    pthread_mutex_lock(&music_mode_mutex);
    music_mode_active = 1;
    pthread_mutex_unlock(&music_mode_mutex);
    led_on();
    buzzer_play_music(0);
    int seconds = 9;
    while (seconds >= 0) {
        pthread_mutex_lock(&music_mode_mutex);
        int active = music_mode_active;
        pthread_mutex_unlock(&music_mode_mutex);
        if (!active) break;
        seg7_display(seconds > 9 ? 9 : seconds);
        sleep(1);
        seconds--;
    }
    led_off();
    seg7_off();
    pthread_mutex_lock(&music_mode_mutex);
    music_mode_active = 0;
    pthread_mutex_unlock(&music_mode_mutex);
    return NULL;
}

void* timer_off_thread(void* arg) {
    int seconds = *(int*)arg;
    free(arg);
    sleep(seconds);
    led_off();
    buzzer_off();
    seg7_off();
    syslog(LOG_INFO, "TIMER 예약 %d초 후 ALL_OFF 실행", seconds);
    return NULL;
}

// 함수 선언(프로토타입)
void handle_signal(int sig);
void daemonize(void);
void setup_gpio(void);
void setup_server(void);
void handle_client(int client_socket);
void cleanup(void);
void write_to_gpio(int pin, int value);
int read_from_gpio(int pin);
void load_device_libs(void);
void close_device_libs(void);
void button_isr(void);

int main(void) {
    // 클라이언트 배열 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }
    // wiringPiSetupGpio()를 main에서 단 한 번만 호출
    wiringPiSetupGpio();
    // 시그널 핸들러 등록
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    // 데몬화
    daemonize();
    // syslog 초기화
    openlog("gpio_daemon", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "GPIO 데몬 시작");
    // GPIO 초기화
    setup_gpio();
    // 버튼 인터럽트 등록 (wiringPiISR)
    if (wiringPiISR(BUTTON_PIN, INT_EDGE_RISING, &button_isr) < 0) {
        syslog(LOG_ERR, "버튼 인터럽트 등록 실패");
        exit(EXIT_FAILURE);
    }
    // TCP 서버 설정
    setup_server();
    // 동적 라이브러리 로드
    load_device_libs();
    // 여기까지 실행되지 않음
    cleanup();
    close_device_libs();
    return 0;
}

// 데몬 프로세스화 함수
void daemonize(void) {
    pid_t pid;
    
    // fork하고 부모 종료
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    // 새 세션 생성
    if (setsid() < 0) exit(EXIT_FAILURE);
    
    // 제어 터미널이 아님을 보장하기 위해 다시 fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    // umask 설정
    umask(0);
    
    // 작업 디렉토리를 루트로 변경
    chdir("/");
    
    // 모든 파일 디스크립터 닫기
    for (int i = 0; i < 1024; i++) {
        close(i);
    }
    
    // 표준 파일 디스크립터를 /dev/null로 리다이렉션
    int null_fd = open("/dev/null", O_RDWR);
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);
    if (null_fd > 2) close(null_fd);
}

// GPIO 초기화 함수
void setup_gpio(void) {
    pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, 0);
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, 0);
    pinMode(SENSOR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);
    pinMode(SEG_A_PIN, OUTPUT); digitalWrite(SEG_A_PIN, 0);
    pinMode(SEG_B_PIN, OUTPUT); digitalWrite(SEG_B_PIN, 0);
    pinMode(SEG_C_PIN, OUTPUT); digitalWrite(SEG_C_PIN, 0);
    pinMode(SEG_D_PIN, OUTPUT); digitalWrite(SEG_D_PIN, 0);
}

// 서버 소켓 설정 및 클라이언트 연결 처리 함수
void setup_server(void) {
    int sockfd;
    struct sockaddr_in serv_addr;
    
    // 소켓 생성
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "소켓 생성 실패");
        exit(EXIT_FAILURE);
    }
    
    // 소켓 옵션 설정
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "소켓 옵션 설정 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // 서버 주소 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    // 소켓 바인딩
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "소켓 바인딩 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // 소켓 리슨
    if (listen(sockfd, BACKLOG) < 0) {
        syslog(LOG_ERR, "소켓 리슨 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    syslog(LOG_INFO, "서버 리슨 포트: %d", SERVER_PORT);
    
    // 클라이언트 연결 처리
    while (1) {
        int client_socket, addr_len;
        struct sockaddr_in client_addr;
        
        addr_len = sizeof(client_addr);
        client_socket = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
        
        if (client_socket < 0) {
            syslog(LOG_ERR, "클라이언트 연결 실패");
            continue;
        }
        
        syslog(LOG_INFO, "새 클라이언트 접속: %s", inet_ntoa(client_addr.sin_addr));
        
        // 클라이언트 소켓을 배열에 추가
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] < 0) {
                client_sockets[i] = client_socket;
                break;
            }
        }
        
        // 클라이언트 처리용 스레드 생성
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, (void *)handle_client, (void *)(intptr_t)client_socket) != 0) {
            syslog(LOG_ERR, "클라이언트 스레드 생성 실패");
            close(client_socket);
        }
        
        // 스레드 분리하여 자동 정리되도록 설정
        pthread_detach(client_thread);
    }
    
    close(sockfd);
}

// 클라이언트 명령 처리 함수
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        syslog(LOG_INFO, "명령 수신: %s", buffer);
        char *cmd = strtok(buffer, ":");
        if (cmd != NULL) {
            if (strcmp(cmd, "LED") == 0) {
                char *subcmd = strtok(NULL, ":");
                if (subcmd) {
                    if (strcmp(subcmd, "ON") == 0) {
                        led_on();
                        write(client_socket, "OK:LED:ON\n", 10);
                    } else if (strcmp(subcmd, "OFF") == 0) {
                        led_off();
                        write(client_socket, "OK:LED:OFF\n", 11);
                    } else if (strcmp(subcmd, "BRIGHT") == 0) {
                        char *level_str = strtok(NULL, ":");
                        if (level_str) {
                            int level = atoi(level_str);
                            led_set_brightness(level);
                            char resp[32];
                            sprintf(resp, "OK:LED:BRIGHT:%d\n", level);
                            write(client_socket, resp, strlen(resp));
                        }
                    }
                }
            } else if (strcmp(cmd, "BUZZER") == 0) {
                char *subcmd = strtok(NULL, ":");
                if (subcmd) {
                    if (strcmp(subcmd, "ON") == 0) {
                        buzzer_on();
                        write(client_socket, "OK:BUZZER:ON\n", 14);
                    } else if (strcmp(subcmd, "OFF") == 0) {
                        buzzer_off();
                        write(client_socket, "OK:BUZZER:OFF\n", 15);
                    } else if (strcmp(subcmd, "MUSIC") == 0) {
                        char *music_id_str = strtok(NULL, ":");
                        int music_id = 1;
                        if (music_id_str) {
                            music_id = atoi(music_id_str);
                            if (music_id < 1 || music_id > 2) music_id = 1;
                        }
                        buzzer_play_music(music_id);
                        write(client_socket, "OK:BUZZER:MUSIC\n", 17);
                    }
                }
            } else if (strcmp(cmd, "SEG7") == 0) {
                char *subcmd = strtok(NULL, ":");
                if (subcmd) {
                    if (strcmp(subcmd, "OFF") == 0) {
                        seg7_off();
                        write(client_socket, "OK:SEG7:OFF\n", 13);
                    } else {
                        int num = atoi(subcmd);
                        seg7_display(num);
                        char resp[32];
                        sprintf(resp, "OK:SEG7:%d\n", num);
                        write(client_socket, resp, strlen(resp));
                    }
                }
            } else if (strcmp(cmd, "EXTRA_MUSIC_MODE") == 0) {
                pthread_mutex_lock(&music_mode_mutex);
                int active = music_mode_active;
                pthread_mutex_unlock(&music_mode_mutex);
                if (!active) {
                    pthread_t t;
                    pthread_create(&t, NULL, music_mode_thread, NULL);
                    pthread_detach(t);
                    write(client_socket, "OK:EXTRA_MUSIC_MODE:START\n", 25);
                } else {
                    pthread_mutex_lock(&music_mode_mutex);
                    music_mode_active = 0;
                    pthread_mutex_unlock(&music_mode_mutex);
                    write(client_socket, "OK:EXTRA_MUSIC_MODE:STOP\n", 24);
                }
            } else if (strcmp(cmd, "SENSOR") == 0) {
                char *pin_str = strtok(NULL, ":");
                if (pin_str != NULL) {
                    int pin = atoi(pin_str);
                    int value = light_sensor_read();
                    // 센서값 응답 전송
                    char response[64];
                    sprintf(response, "VALUE:SENSOR:%d:%d\n", pin, value);
                    write(client_socket, response, strlen(response));
                    syslog(LOG_INFO, "센서 핀 %d 값: %d", pin, value);
                }
            } else if (strcmp(cmd, "ALL_OFF") == 0) {
                led_off();
                buzzer_off();
                seg7_off();
                write(client_socket, "OK:ALL_OFF\n", 11);
                syslog(LOG_INFO, "ALL_OFF 명령으로 모든 디바이스 OFF");
            } else if (strcmp(cmd, "TIMER") == 0) {
                char *sec_str = strtok(NULL, ":");
                if (sec_str) {
                    int *seconds = malloc(sizeof(int));
                    *seconds = atoi(sec_str);
                    pthread_t t;
                    pthread_create(&t, NULL, timer_off_thread, seconds);
                    pthread_detach(t);
                    char resp[64];
                    sprintf(resp, "OK:TIMER:%d\n", *seconds);
                    write(client_socket, resp, strlen(resp));
                    syslog(LOG_INFO, "TIMER 예약 %d초 후 ALL_OFF 예약", *seconds);
                }
            }
        }
    }
    
    // 클라이언트 연결 종료
    syslog(LOG_INFO, "클라이언트 연결 종료");
    
    // 클라이언트 배열 정리
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = -1;
            break;
        }
    }
    
    close(client_socket);
}

// 자원 정리 함수
void cleanup(void) {
    // 모든 클라이언트 소켓 닫기
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
        }
    }
    
    syslog(LOG_INFO, "GPIO 데몬 종료");
    closelog();
}

// 시그널 핸들러 함수
void handle_signal(int sig) {
    syslog(LOG_INFO, "시그널 %d 수신, 종료합니다", sig);
    cleanup();
    exit(EXIT_SUCCESS);
}

// GPIO 출력 함수
void write_to_gpio(int pin, int value) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
}

// GPIO 입력 함수
int read_from_gpio(int pin) {
    pinMode(pin, INPUT);
    return digitalRead(pin);
}

void load_device_libs() {
    led_lib = dlopen("./led.so", RTLD_LAZY);
    if (!led_lib) syslog(LOG_ERR, "led.so 로드 실패: %s", dlerror());

    buzzer_lib = dlopen("./buzzer.so", RTLD_LAZY);
    if (!buzzer_lib) syslog(LOG_ERR, "buzzer.so 로드 실패: %s", dlerror());

    sensor_lib = dlopen("./light_sensor.so", RTLD_LAZY);
    if (!sensor_lib) syslog(LOG_ERR, "light_sensor.so 로드 실패: %s", dlerror());

    seg7_lib = dlopen("./seg7.so", RTLD_LAZY);
    if (!seg7_lib) syslog(LOG_ERR, "seg7.so 로드 실패: %s", dlerror());
}

void close_device_libs() {
    if (led_lib) dlclose(led_lib);
    if (buzzer_lib) dlclose(buzzer_lib);
    if (sensor_lib) dlclose(sensor_lib);
    if (seg7_lib) dlclose(seg7_lib);
}

// 버튼 인터럽트 콜백 함수
void button_isr(void) {
    unsigned long now = millis();
    if (now - last_button_time < 200) return; // 200ms 이내 재진입 방지(디바운스)
    last_button_time = now;

    syslog(LOG_INFO, "버튼 인터럽트 발생!");
    pthread_mutex_lock(&music_mode_mutex);
    int active = music_mode_active;
    pthread_mutex_unlock(&music_mode_mutex);
    if (active) {
        pthread_mutex_lock(&music_mode_mutex);
        music_mode_active = 0;
        pthread_mutex_unlock(&music_mode_mutex);
    } else {
        pthread_t t;
        pthread_create(&t, NULL, music_mode_thread, NULL);
        pthread_detach(t);
    }
    // 기존 클라이언트 알림 메시지 유지
    char msg[64];
    sprintf(msg, "EVENT:BUTTON:%d:1\n", BUTTON_PIN);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            write(client_sockets[i], msg, strlen(msg));
        }
    }
}
