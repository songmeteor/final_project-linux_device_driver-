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
#include "oled.h"

// ===================================================================
//                        사용자 설정
// ===================================================================
const char *playlist[] = {
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/golden.mp3",
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/NinjaTuna128k.mp3",
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/Tsukimichi.mp3",
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/LIKEYOUBETTER.mp3",
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/FAMOUS.mp3",
    "/home/pi43/final_project-linux_device_driver-/user_space/music_vol_ctrl/music/DirtyWork.mp3"

};

const int song_durations[] = { 259, 180, 240, 210, 220, 185 }; // 각 곡의 총 재생 시간 (초)
const int num_tracks = sizeof(playlist) / sizeof(playlist[0]);

const char *vs10xx_dev_path = "/dev/vs10xx-0";
const char *rotary_dev_path = "/dev/rotary_encoder";
const char *oled_dev_path = "/dev/oled";
#define CLICK_TIMEOUT_MS 300
// ===================================================================

typedef enum { STATE_PLAYING, STATE_PAUSED } PlaybackState;
typedef struct {
    int rotary_count;
    int track_current;
    PlaybackState play_state;
    int song_current_sec;
} SharedState;

SharedState player_state;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t player_cond = PTHREAD_COND_INITIALIZER;
volatile int keep_running_threads = 1;
volatile int request_track_change = 0;
// ------------------------------------

void *playback_thread_func(void *arg);
void *control_thread_func(void *arg);
void *ui_thread_func(void *arg);
long get_time_diff_ms(struct timespec* start, struct timespec* end);

int main(void) {
    pthread_t playback_tid, control_tid, ui_tid;

    printf("Starting Integrated MP3 Player...\n");
    player_state.rotary_count = 0;
    player_state.track_current = 0;
    player_state.play_state = STATE_PLAYING;
    player_state.song_current_sec = 0;

    pthread_create(&playback_tid, NULL, playback_thread_func, NULL);
    pthread_create(&control_tid, NULL, control_thread_func, NULL);
    pthread_create(&ui_tid, NULL, ui_thread_func, NULL);

    pthread_join(playback_tid, NULL);
    keep_running_threads = 0;
    pthread_cond_signal(&player_cond);
    pthread_join(control_tid, NULL);
    pthread_join(ui_tid, NULL);

    printf("Program terminated.\n");
    return 0;
}

// ===================================================================
//                        재생 스레드
// ===================================================================

void *playback_thread_func(void *arg) {
    int vs10xx_fd = open(vs10xx_dev_path, O_WRONLY);
    if (vs10xx_fd < 0) { perror("Playback: Failed to open vs10xx"); return NULL; }
    
    while (keep_running_threads) {
        pthread_mutex_lock(&state_mutex);
        int track_to_play = player_state.track_current;
        pthread_mutex_unlock(&state_mutex);

        const char* file_to_play = playlist[track_to_play];
        FILE *fptr = fopen(file_to_play, "rb");
        if (fptr == NULL) { /* ... */ continue; }

        printf("\n▶? Now Playing: %s\n", file_to_play);
        
        long file_size = 0;
        fseek(fptr, 0, SEEK_END);
        file_size = ftell(fptr);
        fseek(fptr, 0, SEEK_SET);

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0) {
            pthread_mutex_lock(&state_mutex);
            while (player_state.play_state == STATE_PAUSED && !request_track_change) {
                pthread_cond_wait(&player_cond, &state_mutex);
            }
            if (request_track_change || !keep_running_threads) {
                pthread_mutex_unlock(&state_mutex);
                break;
            }
            pthread_mutex_unlock(&state_mutex);
            
            if (write(vs10xx_fd, buffer, bytes_read) < 0) break;
            
            pthread_mutex_lock(&state_mutex);
            if (file_size > 0) {
                player_state.song_current_sec = (ftell(fptr) * song_durations[track_to_play]) / file_size;
            }
            pthread_mutex_unlock(&state_mutex);
        }
        fclose(fptr);

        pthread_mutex_lock(&state_mutex);
        if (request_track_change) {
            request_track_change = 0;
        } else {
            player_state.track_current = (player_state.track_current + 1) % num_tracks;
        }
        pthread_mutex_unlock(&state_mutex);
    }
    close(vs10xx_fd);
    printf("Playback thread finished.\n");
    return NULL;
}


// ===================================================================
//                        제어 스레드
// ===================================================================
void *control_thread_func(void *arg) {
    int rotary_fd = open(rotary_dev_path, O_RDONLY);
    int vs10xx_fd = open(vs10xx_dev_path, O_WRONLY);
    int current_count, key_event;
    char read_buf[32];
    int click_count = 0;
    struct timespec last_click_time = {0, 0};
    
    if (rotary_fd < 0 || vs10xx_fd < 0) { /* ... */ return NULL; }
    printf("Control thread started.\n");

    while (keep_running_threads) {
        lseek(rotary_fd, 0, SEEK_SET);
        if (read(rotary_fd, read_buf, sizeof(read_buf) - 1) > 0) {
            if (sscanf(read_buf, "%d,%d", &current_count, &key_event) == 2) {
                pthread_mutex_lock(&state_mutex);
                if(player_state.rotary_count != current_count) {
                    player_state.rotary_count = current_count;
                    int new_volume = 100 - (current_count * 5);
                    if (new_volume < 0) new_volume = 0;
                    if (new_volume > 100) new_volume = 100;
                    unsigned int packed_vol = (new_volume << 8) | new_volume;
                    ioctl(vs10xx_fd, VS10XX_SET_VOL, &packed_vol);
                }
                pthread_mutex_unlock(&state_mutex);

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
                pthread_mutex_lock(&state_mutex);
                if (click_count == 1) { player_state.play_state = !player_state.play_state; } 
                else if (click_count == 2) { request_track_change = 1; player_state.track_current = (player_state.track_current + 1) % num_tracks; }
                else if (click_count >= 3) { request_track_change = 1; player_state.track_current = (player_state.track_current - 1 + num_tracks) % num_tracks; }
                if(player_state.play_state == STATE_PLAYING) pthread_cond_signal(&player_cond);
                pthread_mutex_unlock(&state_mutex);
                click_count = 0;
            }
        }
        usleep(50000);
    }
    close(rotary_fd);
    close(vs10xx_fd);
    printf("Control thread finished.\n");
    return NULL;
}


// ===================================================================
//                        UI 업데이트 스레드
// ===================================================================
void *ui_thread_func(void *arg) {
    int oled_fd = open(oled_dev_path, O_WRONLY);
    if (oled_fd < 0) { perror("UI: Failed to open oled"); return NULL; }
    
    struct mp3_ui_data ui_data;
    
    while (keep_running_threads) {
        // --- 공유 변수에서 UI 데이터로 안전하게 복사 ---
        pthread_mutex_lock(&state_mutex);
        int track_idx = player_state.track_current;
        int count = player_state.rotary_count;
        int current_sec = player_state.song_current_sec;
        int spectrum_run_stop = player_state.play_state;
        pthread_mutex_unlock(&state_mutex);
        
        // --- UI 데이터 구조체 채우기 ---
        memset(&ui_data, 0, sizeof(ui_data));
        
        // 1. 볼륨
        ui_data.volume = count; // 0-100을 0-15로 변환
        
        // 2. 현재 시간
        time_t t = time(NULL); struct tm *tm = localtime(&t);
        snprintf(ui_data.current_time, sizeof(ui_data.current_time), "%02d:%02d", tm->tm_hour, tm->tm_min);
        
        // 3. 트랙 정보
        ui_data.track_current = track_idx + 1;
        ui_data.track_total = num_tracks;
        
        ui_data.spectrum_run_stop = spectrum_run_stop;

        // 5. 재생 시간
        snprintf(ui_data.playback_time, sizeof(ui_data.playback_time), "%02d:%02d", current_sec / 60, current_sec % 60);
        
        // 6. 곡 제목 (경로에서 파일 이름만 추출)
        const char *last_slash = strrchr(playlist[track_idx], '/');
        if (last_slash) {
            strncpy(ui_data.song_title, last_slash + 1, sizeof(ui_data.song_title) - 1);
        } else {
            strncpy(ui_data.song_title, playlist[track_idx], sizeof(ui_data.song_title) - 1);
        }
        
        // 7. 전체 시간
        int total_sec = song_durations[track_idx];
        snprintf(ui_data.total_time, sizeof(ui_data.total_time), "%02d:%02d", total_sec / 60, total_sec % 60);

        // --- OLED 드라이버로 데이터 전송 ---
        if (write(oled_fd, &ui_data, sizeof(ui_data)) < 0) {
            perror("UI: Failed to write to oled device");
        }
        
        usleep(100000); // 0.1초마다 화면 업데이트
    }
    
    close(oled_fd);
    printf("UI thread finished.\n");
    return NULL;
}

long get_time_diff_ms(struct timespec* start, struct timespec* end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1000000;
}