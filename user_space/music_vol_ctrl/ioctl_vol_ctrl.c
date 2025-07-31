/*
 * 파일: ioctl.c (최종 버전)
 * 멀티 스레딩으로 재생과 볼륨 조절을 동시에 처리합니다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>   // --- 스레딩을 위한 헤더파일 추가 ---
#include "vs10xx.h"

// --- 장치 및 파일 경로 설정 ---
const char *mp3_filename = "/home/pi43/music/golden.mp3";
const char *vs10xx_dev_path = "/dev/vs10xx-0";
const char *rotary_dev_path = "/dev/rotary_encoder"; // <-- 로터리 엔코더 장치 파일
// -----------------------------

// 스레드 종료를 제어하는 전역 플래그
volatile int keep_running_volume_thread = 0;

// 로터리 엔코더 count 값을 VS10xx 볼륨 값(0-254)으로 변환하는 함수
unsigned char map_count_to_volume(int count) {
    // count가 1 증가/감소할 때마다 볼륨을 5씩 조절, 기본 볼륨 100
    int new_volume = 100 - (count * 5);
    
    if (new_volume < 0) new_volume = 0;     // 0 = 최대 볼륨
    if (new_volume > 254) new_volume = 254; // 254 = 최소 볼륨 (음소거)
    return (unsigned char)new_volume;
}

// --- 볼륨 조절 스레드가 실행할 함수 ---
void *volume_control_thread_func(void *arg) {
    int rotary_fd = open(rotary_dev_path, O_RDONLY);
    int vs10xx_fd = open(vs10xx_dev_path, O_WRONLY);
    int last_count = -999, current_count;
    char read_buf[16];

    if (rotary_fd < 0 || vs10xx_fd < 0) {
        perror("Volume Thread: Failed to open device(s)");
        return NULL;
    }

    printf("Volume control thread started.\n");

    while (keep_running_volume_thread) {
        lseek(rotary_fd, 0, SEEK_SET); // 항상 파일의 처음부터 값을 읽음
        ssize_t bytes = read(rotary_fd, read_buf, sizeof(read_buf) - 1);
        
        if (bytes > 0) {
            read_buf[bytes] = '\0';
            current_count = atoi(read_buf);

            if (current_count != last_count) {
                unsigned char volume = map_count_to_volume(current_count);
                unsigned int packed_volume = (volume << 8) | volume;
                
                if (ioctl(vs10xx_fd, VS10XX_SET_VOL, &packed_volume) == 0) {
                    printf("Rotary count: %d -> Volume set to: %d\n", current_count, volume);
                }
                last_count = current_count;
            }
        }
        usleep(100000); // 0.1초마다 체크하여 CPU 부하 줄임
    }

    printf("Volume control thread stopped.\n");
    close(rotary_fd);
    close(vs10xx_fd);
    return NULL;
}


void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <command> [params...]\n", prog_name);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  setvolume <left> <right>  (0=loudest, 254=quietest)\n");
    fprintf(stderr, "  play                      Play MP3 with volume control\n\n");
}

int main(int argc, char *argv[]) {
    int fd;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "setvolume") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s setvolume <left> <right>\n", argv[0]);
            return 1;
        }
        fd = open(vs10xx_dev_path, O_RDWR);
        if (fd < 0) {
            perror("Error opening vs10xx device");
            return 1;
        }
        int left = atoi(argv[2]);
        int right = atoi(argv[3]);
        unsigned int packed_volume = (left << 8) | right;

        printf("Setting volume: Left=%d, Right=%d\n", left, right);
        if (ioctl(fd, VS10XX_SET_VOL, &packed_volume) < 0) {
            perror("ioctl(VS10XX_SET_VOL) failed");
        }
        close(fd);

    } else if (strcmp(command, "play") == 0) {
        pthread_t volume_thread_id;
        
        // 1. 볼륨 조절 스레드 시작
        keep_running_volume_thread = 1;
        if (pthread_create(&volume_thread_id, NULL, volume_control_thread_func, NULL) != 0) {
            perror("Failed to create volume control thread");
            return 1;
        }

        // 2. 메인 스레드에서 음악 재생
        fd = open(vs10xx_dev_path, O_WRONLY);
        FILE *fptr = fopen(mp3_filename, "rb");
        if (fd < 0 || fptr == NULL) {
            perror("Main Thread: Failed to open file for playback");
            keep_running_volume_thread = 0; // 스레드 종료 신호
        } else {
            char buffer[4096];
            size_t bytes_read;
            printf("Main Thread: Playing %s... (Control volume with rotary encoder)\n", mp3_filename);
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0) {
                if (write(fd, buffer, bytes_read) < 0) {
                    perror("Main Thread: Failed to write to device");
                    break;
                }
            }
            printf("Main Thread: Playback finished.\n");
            fclose(fptr);
            close(fd);
        }

        // 3. 재생이 끝나면 볼륨 조절 스레드 종료 요청
        keep_running_volume_thread = 0;
        pthread_join(volume_thread_id, NULL); // 스레드가 완전히 끝날 때까지 대기
        printf("Program finished.\n");

    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
    }

    return 0;
}
