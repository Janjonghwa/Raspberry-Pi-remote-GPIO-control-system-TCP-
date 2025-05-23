/**
 * gpio_client.c - 클라이언트 GPIO 제어 및 상태 확인 프로그램
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_PORT 5000
#define BUFFER_SIZE 1024
#define LED_PIN 17
#define BUTTON_PIN 18
#define SENSOR_PIN 19
#define BUZZER_PIN 27

#define COLOR_RESET   "\033[0m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_RED     "\033[31m"
#define COLOR_BOLD    "\033[1m"

// 함수 선언
void control_led(int socket, int pin, int state);
void read_button(int socket, int pin);
void read_sensor(int socket, int pin);
void display_digit(int socket, int digit);
void *receive_updates(void *arg);
void handle_signal(int sig);
void control_buzzer(int socket, int pin);
void device_self_test(int sockfd);

void print_menu() {
    printf(COLOR_CYAN);
    printf("\n========================================\n");
    printf(COLOR_BOLD "  Raspberry Pi 원격 GPIO 제어 메뉴  " COLOR_RESET COLOR_CYAN "\n");
    printf("========================================\n");
    printf(COLOR_YELLOW "[1]" COLOR_RESET "  💡  LED ON\n");
    printf(COLOR_YELLOW "[2]" COLOR_RESET "  💡  LED OFF\n");
    printf(COLOR_YELLOW "[3]" COLOR_RESET "  💡  LED 밝기 설정\n");
    printf(COLOR_YELLOW "[4]" COLOR_RESET "  🔔  BUZZER ON\n");
    printf(COLOR_YELLOW "[5]" COLOR_RESET "  🔕  BUZZER OFF\n");
    printf(COLOR_YELLOW "[6]" COLOR_RESET "  🎵  BUZZER 음악 재생\n");
    printf(COLOR_YELLOW "[7]" COLOR_RESET "  7️⃣  7-Segment 숫자 표시\n");
    printf(COLOR_YELLOW "[8]" COLOR_RESET "  7️⃣  7-Segment OFF\n");
    printf(COLOR_YELLOW "[9]" COLOR_RESET "  🌞  조도센서 값 읽기\n");
    printf(COLOR_YELLOW "[10]" COLOR_RESET "  ⭐  추가기능(버튼+음악+LED+세그먼트)\n");
    printf(COLOR_YELLOW "[11]" COLOR_RESET "  🚨  전체 OFF (ALL_OFF)\n");
    printf(COLOR_YELLOW "[12]" COLOR_RESET "  ⏰  예약 OFF (TIMER)\n");
    printf(COLOR_YELLOW "[0]" COLOR_RESET "  ❌  종료\n");
    printf("----------------------------------------\n");
    printf(COLOR_GREEN "원하는 기능의 번호를 입력하세요: " COLOR_RESET);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    pthread_t update_thread;
    struct sigaction sa;
    
    // 인자 확인
    if (argc < 2) {
        fprintf(stderr, COLOR_RED "사용법: %s <서버_IP>\n" COLOR_RESET, argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // 소켓 생성
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }
    
    // 서버 정보 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // IP 주소를 직접 사용하여 변환
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, COLOR_RED "유효하지 않은 IP 주소입니다: %s\n" COLOR_RESET, argv[1]);
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_port = htons(SERVER_PORT);
    
    // 연결 시도
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("연결 실패");
        exit(EXIT_FAILURE);
    }
    
    printf(COLOR_GREEN "서버 %s에 연결됨\n" COLOR_RESET, argv[1]);
    
    // 업데이트 스레드 생성
    if (pthread_create(&update_thread, NULL, receive_updates, &sockfd) != 0) {
        perror("업데이트 스레드 생성 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // 사용자 선택
    int choice, level, num, timer_sec;
    
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    for (int i = 1; i < NSIG; i++) {
        sigaction(i, &sa, NULL);
    }
    
    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            getchar(); // 입력 버퍼 비우기
            continue;
        }
        
        char command[64];
        switch (choice) {
            case 1: // LED ON
                strcpy(command, "LED:ON");
                write(sockfd, command, strlen(command));
                break;
                
            case 2: // LED OFF
                strcpy(command, "LED:OFF");
                write(sockfd, command, strlen(command));
                break;
                
            case 3: // LED 밝기 설정
                printf(COLOR_MAGENTA "밝기 (0=최소, 1=중간, 2=최대): " COLOR_RESET);
                scanf("%d", &level);
                sprintf(command, "LED:BRIGHT:%d", level);
                write(sockfd, command, strlen(command));
                break;
                
            case 4: // BUZZER ON
                strcpy(command, "BUZZER:ON");
                write(sockfd, command, strlen(command));
                break;
                
            case 5: // BUZZER OFF
                strcpy(command, "BUZZER:OFF");
                write(sockfd, command, strlen(command));
                break;
                
            case 6: // BUZZER 음악 재생
                printf(COLOR_MAGENTA "\n[BUZZER 음악 선택] 1=곰 세 마리, 2=아이돌\n" COLOR_RESET);
                printf(COLOR_GREEN "음악 번호를 입력하세요 (1/2): " COLOR_RESET);
                int music_sel = 1;
                scanf("%d", &music_sel);
                if (music_sel != 2) music_sel = 1;
                sprintf(command, "BUZZER:MUSIC:%d", music_sel);
                write(sockfd, command, strlen(command));
                break;
                
            case 7: // 7-Segment 숫자 표시
                printf(COLOR_MAGENTA "숫자 (0-9): " COLOR_RESET);
                scanf("%d", &num);
                sprintf(command, "SEG7:%d", num);
                write(sockfd, command, strlen(command));
                break;
                
            case 8: // 7-Segment OFF
                strcpy(command, "SEG7:OFF");
                write(sockfd, command, strlen(command));
                break;
                
            case 9: // 조도센서 값 읽기
                strcpy(command, "SENSOR:27");
                write(sockfd, command, strlen(command));
                break;
                
            case 10: // 추가기능(버튼+음악+LED+세그먼트)
                strcpy(command, "EXTRA_MUSIC_MODE");
                write(sockfd, command, strlen(command));
                break;
                
            case 11: // 전체 OFF (ALL_OFF)
                strcpy(command, "ALL_OFF");
                write(sockfd, command, strlen(command));
                break;
                
            case 12: // 예약 OFF (TIMER)
                printf(COLOR_MAGENTA "예약 OFF 시간(초): " COLOR_RESET);
                scanf("%d", &timer_sec);
                sprintf(command, "TIMER:%d", timer_sec);
                write(sockfd, command, strlen(command));
                break;
                
            case 0: // 종료
                printf(COLOR_RED "프로그램을 종료합니다.\n" COLOR_RESET);
                close(sockfd);
                return 0;
                
            default:
                printf(COLOR_RED "잘못된 선택입니다. 다시 입력해주세요.\n" COLOR_RESET);
        }
    }
    
    return 0;
}

void control_led(int socket, int pin, int state) {
    char command[64];
    char buffer[BUFFER_SIZE];
    
    sprintf(command, "LED:%d:%d", pin, state);
    
    // 서버로 명령 전송
    if (write(socket, command, strlen(command)) < 0) {
        perror("명령 전송 실패");
        return;
    }
    
    // 응답 확인 (응답이 필요한 경우)
    /*
    int n = read(socket, buffer, BUFFER_SIZE - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("서버 응답: %s\n", buffer);
    }
    */
}

void read_button(int socket, int pin) {
    char command[64];
    
    sprintf(command, "BTN:%d", pin);
    
    // 서버로 명령 전송
    if (write(socket, command, strlen(command)) < 0) {
        perror("명령 전송 실패");
        return;
    }
    
    // 응답 확인을 위해 대기
}

void read_sensor(int socket, int pin) {
    char command[64];
    
    sprintf(command, "SENSOR:%d", pin);
    
    // 서버로 명령 전송
    if (write(socket, command, strlen(command)) < 0) {
        perror("명령 전송 실패");
        return;
    }
    
    // 응답 확인을 위해 대기
}

void display_digit(int socket, int digit) {
    char command[64];
    
    sprintf(command, "7SEG:%d", digit);
    
    // 서버로 명령 전송
    if (write(socket, command, strlen(command)) < 0) {
        perror("명령 전송 실패");
        return;
    }
    
    // 응답 확인을 위해 대기
}

void *receive_updates(void *arg) {
    int sockfd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    while (1) {
        int n = read(sockfd, buffer, BUFFER_SIZE - 1);
        if (n > 0) {
            buffer[n] = '\0';
            if (strstr(buffer, "EVENT:BUTTON") != NULL) {
                printf(COLOR_YELLOW "[알림] 버튼이 눌렸습니다!\n" COLOR_RESET);
            } else {
                printf(COLOR_GREEN "서버 응답: %s" COLOR_RESET, buffer);
            }
        }
    }
    return NULL;
}

void handle_signal(int sig) {
    printf("\n클라이언트 종료\n");
    exit(0);
}

void control_buzzer(int socket, int pin) {
    char command[64];
    sprintf(command, "BUZZER:%d", pin);
    if (write(socket, command, strlen(command)) < 0) {
        perror("명령 전송 실패");
        return;
    }
}

void device_self_test(int sockfd) {
    printf("LED 테스트: ON\n");
    control_led(sockfd, LED_PIN, 1);
    sleep(1);
    printf("LED 테스트: OFF\n");
    control_led(sockfd, LED_PIN, 0);
    sleep(1);

    printf("부저 테스트\n");
    control_buzzer(sockfd, BUZZER_PIN);
    sleep(1);

    printf("7-Segment 테스트: 3\n");
    display_digit(sockfd, 3);
    sleep(2);

    printf("센서 값 읽기: ");
    read_sensor(sockfd, SENSOR_PIN);
    sleep(1);

    printf("버튼 상태 읽기: ");
    read_button(sockfd, BUTTON_PIN);
    sleep(1);

    printf("진단 완료. 각 장치의 실제 반응을 확인하세요.\n");
}
