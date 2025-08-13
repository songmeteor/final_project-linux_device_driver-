// oled_test.c (애니메이션 테스트 버전)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>      // usleep 함수를 위해 추가
#include "oled.h"

#define DEVICE_PATH "/dev/oled"

int main() {
    int fd;
    struct mp3_ui_data ui_data;
    int i; // 반복문을 위한 변수

    // 디바이스 파일 열기
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open the device file");
        return -1;
    }

    // --- 기본 UI 데이터 설정 (한 번만 설정) ---
    memset(&ui_data, 0, sizeof(struct mp3_ui_data));
    ui_data.volume = 11;
    strncpy(ui_data.current_time, "12:04", sizeof(ui_data.current_time) - 1);
    ui_data.track_current = 1;
    ui_data.track_total = 5;
    strncpy(ui_data.playback_time, "00:32", sizeof(ui_data.playback_time) - 1);
    strncpy(ui_data.song_title, "Animation Test", sizeof(ui_data.song_title) - 1);
    strncpy(ui_data.total_time, "02:48", sizeof(ui_data.total_time) - 1);

    printf("Starting spectrum analyzer animation for 10 seconds...\n");

    // --- 100번 반복하며 애니메이션 생성 (100 * 0.1초 = 10초) ---
    for (i = 0; i < 100; i++) {
        // 매번 동일한 데이터를 쓰더라도, 드라이버는 랜덤 값을 다시 생성합니다.
        ssize_t bytes_written = write(fd, &ui_data, sizeof(struct mp3_ui_data));

        if (bytes_written < 0) {
            perror("Failed to write to the device");
            break; // 에러 발생 시 반복 중단
        }

        // 터미널에 진행 상황 출력
        printf(".");
        fflush(stdout); // 버퍼를 비워 "."가 바로 보이도록 함

        // 100,000 마이크로초 (0.1초) 동안 대기
        usleep(200000);
    }
    
    printf("\nAnimation test finished.\n");

    // 디바이스 파일 닫기
    close(fd);

    return 0;
}