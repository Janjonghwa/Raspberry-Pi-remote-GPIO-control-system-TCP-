# Makefile - 빌드 자동화 파일 (서버, 클라이언트, 각 디바이스 동적 라이브러리)

CC = gcc                              # 컴파일러 지정
CFLAGS = -Wall -Wextra -pthread       # 컴파일 옵션 (경고, pthread)
LDFLAGS = -pthread                    # 링크 옵션 (pthread)

SERVER_SRC = gpio_server_daemon.c led.c buzzer.c seg7.c light_sensor.c
CLIENT_SRC = gpio_client.c            # 클라이언트 소스 파일

SERVER = gpio_server_daemon           # 서버 실행 파일명
CLIENT = gpio_client                  # 클라이언트 실행 파일명

LIBS = led.so buzzer.so light_sensor.so seg7.so   # 동적 라이브러리 목록

.PHONY: all clean                     # 가상 타겟 선언

all: $(SERVER) $(CLIENT) $(LIBS)      # 전체 빌드 (서버, 클라이언트, 라이브러리)

$(SERVER): $(SERVER_SRC)              # 서버 빌드 규칙
	$(CC) $(CFLAGS) -o $@ $^ -lwiringPi $(LDFLAGS)

$(CLIENT): $(CLIENT_SRC)              # 클라이언트 빌드 규칙
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(LIBS): %.so: %.c                    # 동적 라이브러리 빌드 규칙
	$(CC) -fPIC -shared -o $@ $<

clean:                                # 빌드 결과물 삭제
	rm -f $(SERVER) $(CLIENT) $(LIBS)
