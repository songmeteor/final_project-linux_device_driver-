#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include "oled_ui.h"

#define DEVICE_PATH "/dev/oled_dev"

int main() {
    int fd;
    struct oled_mp3_ui_data ui_data;
    int current_sec = 0;
    int total_sec = 259; // 4분 19초
    int is_playing = 1;
    int i;

    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device file");
        return 1;
    }

    srand(time(NULL)); // 랜덤 시드 초기화

    strncpy(ui_data.song_title, "Smile.mp3", sizeof(ui_data.song_title) - 1);
    strncpy(ui_data.track_info, "002/028", sizeof(ui_data.track_info) - 1);
    
    printf("Starting MP3 Player UI simulation...\n");

    while (current_sec <= total_sec) {
        // 데이터 업데이트
        snprintf(ui_data.current_time, sizeof(ui_data.current_time), "%02d:%02d", current_sec / 60, current_sec % 60);
        snprintf(ui_data.total_time, sizeof(ui_data.total_time), "%02d:%02d", total_sec / 60, total_sec % 60);
        ui_data.progress = (current_sec * 100) / total_sec;
        ui_data.play_state = is_playing;

        // 시각화 막대 데이터 랜덤 생성
        for(i = 0; i < VISUALIZER_BARS; i++) {
            ui_data.visualizer_bars[i] = rand() % 16; // 0~15 높이
        }

        // ioctl로 드라이버에 데이터 전송
        if (ioctl(fd, OLED_UPDATE_UI, &ui_data) < 0) {
            perror("ioctl failed");
            break;
        }

        usleep(200000); // 0.2초 대기
        if(is_playing) current_sec++;
    }

    printf("Simulation finished.\n");
    close(fd);
    return 0;
}

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <fcntl.h>
// #include <unistd.h>

// #define DEVICE_PATH "/dev/oled_dev"

// int main(int argc, char *argv[]) {
//     int fd;
//     char *write_buf;

//     if (argc < 2) {
//         fprintf(stderr, "Usage: %s <message to display>\n", argv[0]);
//         fprintf(stderr, "       %s -c (to clear screen)\n", argv[0]);
//         return 1;
//     }

//     fd = open(DEVICE_PATH, O_WRONLY);
//     if (fd < 0) {
//         perror("Failed to open device file");
//         return 1;
//     }

//     if (strcmp(argv[1], "-c") == 0) {
//         // 화면을 지우기 위해 폼 피드 문자를 전송
//         write_buf = "\f";
//         write(fd, write_buf, 1);
//         printf("Clear command sent to OLED.\n");
//     } else {
//         write_buf = argv[1];
//         int len = strlen(write_buf);
//         if (write(fd, write_buf, len) != len) {
//             perror("Failed to write to device");
//         } else {
//             printf("Message sent to OLED: %s\n", write_buf);
//         }
//     }

//     close(fd);
//     return 0;
// }