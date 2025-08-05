/*
 * 최종 완성 버전: 모든 기능 통합 및 버그 수정
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "vs10xx.h"

// ===================================================================
//                        사용자 설정
// ===================================================================
const char *playlist[] = {
    "/home/pi43/music/golden.mp3",
    "/home/pi43/music/NinjaTuna128k.mp3",
};
const int num_tracks = sizeof(playlist) / sizeof(playlist[0]);
const char *vs10xx_dev_path = "/dev/vs10xx-0";
const char *rotary_dev_path = "/dev/rotary_encoder";
#define CLICK_TIMEOUT_MS 300
// ===================================================================

volatile int keep_running_threads = 1;
volatile int current_track = 0;
typedef enum { STATE_STOPPED, STATE_PLAYING, STATE_PAUSED } PlaybackState;
volatile PlaybackState playback_state = STATE_STOPPED;
volatile int request_track_change = 0;
pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t player_cond = PTHREAD_COND_INITIALIZER;

unsigned char map_count_to_volume(int count) {
    int new_volume = 100 - (count * 5);
    if (new_volume < 0) new_volume = 0;
    if (new_volume > 254) new_volume = 254;
    return (unsigned char)new_volume;
}

long get_time_diff_ms(struct timespec* start, struct timespec* end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1000000;
}

// ===================================================================
//                        재생 스레드 (수정됨)
// ===================================================================
void *playback_thread_func(void *arg) {
    int fd = open(vs10xx_dev_path, O_WRONLY);
    if (fd < 0) {
        perror("Playback Thread: Failed to open vs10xx device");
        return NULL;
    }

    while (keep_running_threads) {
        pthread_mutex_lock(&player_mutex);
        while (playback_state != STATE_PLAYING && keep_running_threads) {
            pthread_cond_wait(&player_cond, &player_mutex);
        }
        pthread_mutex_unlock(&player_mutex);

        if (!keep_running_threads) break;

        const char* file_to_play = playlist[current_track];
        FILE *fptr = fopen(file_to_play, "rb");
        
        if (fptr == NULL) {
            fprintf(stderr, "Playback Thread: Failed to open %s\n", file_to_play);
            playback_state = STATE_STOPPED;
            continue;
        }

        printf("\n▶? Now Playing: %s\n", file_to_play);
        
        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0) {
            pthread_mutex_lock(&player_mutex);
            while (playback_state == STATE_PAUSED && !request_track_change) {
                pthread_cond_wait(&player_cond, &player_mutex);
            }
            if (request_track_change) {
                pthread_mutex_unlock(&player_mutex);
                break;
            }
            pthread_mutex_unlock(&player_mutex);
            
            if (write(fd, buffer, bytes_read) < 0) {
                perror("Playback Thread: Failed to write to device");
                break;
            }
        }
        fclose(fptr);

        // ===================================================================
        // --- 여기가 수정된 핵심 로직입니다 ---
        pthread_mutex_lock(&player_mutex);
        if (request_track_change) {
            // 곡 변경 요청을 처리했으므로, 플래그를 다시 0으로 리셋합니다.
            request_track_change = 0;
        } else {
            // 곡이 정상적으로 끝났으면 다음 곡으로 자동 이동
            current_track = (current_track + 1) % num_tracks;
        }
        pthread_mutex_unlock(&player_mutex);
        // ===================================================================

        // 다음 곡 재생을 위해 신호 전송
        pthread_cond_signal(&player_cond);
    }

    close(fd);
    printf("Playback thread finished.\n");
    return NULL;
}

// ===================================================================
//                        제어 스레드
// ===================================================================
void *control_thread_func(void *arg) {
    int rotary_fd = open(rotary_dev_path, O_RDONLY);
    int vs10xx_fd = open(vs10xx_dev_path, O_WRONLY);
    
    int last_count = -999, current_count, key_event;
    char read_buf[32];
    int click_count = 0;
    struct timespec last_click_time = {0, 0};

    if (rotary_fd < 0 || vs10xx_fd < 0) {
        perror("Control Thread: Failed to open devices");
        keep_running_threads = 0;
        pthread_cond_signal(&player_cond);
        return NULL;
    }
    printf("Control thread started. Ready for input.\n");

    while (keep_running_threads) {
        lseek(rotary_fd, 0, SEEK_SET);
        ssize_t bytes = read(rotary_fd, read_buf, sizeof(read_buf) - 1);
        if (bytes > 0) {
            read_buf[bytes] = '\0';
            if (sscanf(read_buf, "%d,%d", &current_count, &key_event) == 2) {
                if (current_count != last_count) {
                    unsigned char volume = map_count_to_volume(current_count);
                    unsigned int packed_volume = (volume << 8) | volume;
                    if (ioctl(vs10xx_fd, VS10XX_SET_VOL, &packed_volume) == 0) {
                        printf("Rotary count: %d -> Volume set to: %d\n", current_count, volume);
                    }
                    last_count = current_count;
                }
                if (key_event == 1) {
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    if (get_time_diff_ms(&last_click_time, &now) > CLICK_TIMEOUT_MS) {
                        click_count = 1;
                    } else {
                        click_count++;
                    }
                    last_click_time = now;
                }
            }
        }

        if (click_count > 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (get_time_diff_ms(&last_click_time, &now) > CLICK_TIMEOUT_MS) {
                pthread_mutex_lock(&player_mutex);
                if (click_count == 1) {
                    if (playback_state == STATE_PLAYING) {
                        playback_state = STATE_PAUSED;
                        printf("|| Paused\n");
                    } else {
                        playback_state = STATE_PLAYING;
                        printf("▶? Resumed\n");
                    }
                } else if (click_count == 2) {
                    printf(">> Next Track\n");
                    request_track_change = 1;
                    current_track = (current_track + 1) % num_tracks;
                    playback_state = STATE_PLAYING;
                } else if (click_count >= 3) {
                    printf("<< Previous Track\n");
                    request_track_change = 1;
                    current_track = (current_track - 1 + num_tracks) % num_tracks;
                    playback_state = STATE_PLAYING;
                }
                pthread_cond_signal(&player_cond);
                pthread_mutex_unlock(&player_mutex);
                click_count = 0;
            }
        }
        usleep(10000);
    }
    
    close(rotary_fd);
    close(vs10xx_fd);
    printf("Control thread finished.\n");
    return NULL;
}


// ===================================================================
//                        메인 스레드
// ===================================================================
int main(void) {
    pthread_t playback_thread_id, control_thread_id;

    printf("Starting MP3 Player...\n");

    int initial_fd = open(vs10xx_dev_path, O_WRONLY);
    if(initial_fd > 0) {
        unsigned char vol = map_count_to_volume(0);
        unsigned int p_vol = (vol << 8) | vol;
        ioctl(initial_fd, VS10XX_SET_VOL, &p_vol);
        close(initial_fd);
    }
    
    pthread_create(&playback_thread_id, NULL, playback_thread_func, NULL);
    pthread_create(&control_thread_id, NULL, control_thread_func, NULL);

    pthread_mutex_lock(&player_mutex);
    playback_state = STATE_PLAYING;
    pthread_cond_signal(&player_cond);
    pthread_mutex_unlock(&player_mutex);
    
    pthread_join(playback_thread_id, NULL);
    
    keep_running_threads = 0;
    pthread_join(control_thread_id, NULL);

    printf("Program terminated.\n");
    return 0;
}