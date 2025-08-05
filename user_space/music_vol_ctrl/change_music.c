/*
 * 최종 버전: 다중 곡 재생, 볼륨 조절, 일시정지, 곡 넘기기 기능 구현
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

// --- 재생 목록 ---
const char *playlist[] = {
    "/home/pi43/music/golden.mp3",
    "/home/pi43/music/NinjaTuna128k.mp3",
    // 여기에 더 많은 곡 경로를 추가하세요.
    // 예: "/home/pi43/music/another_song.mp3",
};
const int num_tracks = sizeof(playlist) / sizeof(playlist[0]);

// --- 장치 파일 경로 ---
const char *vs10xx_dev_path = "/dev/vs10xx-0";
const char *rotary_dev_path = "/dev/rotary_encoder";

// --- 클릭 감지 시간 (밀리초) ---
#define CLICK_TIMEOUT_MS 300

// ===================================================================

// --- 스레드 간 통신을 위한 전역 변수 ---
volatile int keep_running_threads = 1;
volatile int current_track = 0;

// 재생 상태 제어
typedef enum { STATE_STOPPED, STATE_PLAYING, STATE_PAUSED } PlaybackState;
volatile PlaybackState playback_state = STATE_STOPPED;
volatile int request_track_change = 0; // 1: 다음 곡, -1: 이전 곡

// 스레드 동기화를 위한 Mutex와 Condition Variable
pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t player_cond = PTHREAD_COND_INITIALIZER;
// ------------------------------------

// 볼륨 변환 함수
unsigned char map_count_to_volume(int count) {
    int new_volume = 100 - (count * 5);
    if (new_volume < 0) new_volume = 0;
    if (new_volume > 254) new_volume = 254;
    return (unsigned char)new_volume;
}

// 나노초 단위 시간 차이를 계산하는 함수
long get_time_diff_ms(struct timespec* start, struct timespec* end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1000000;
}


// ===================================================================
//                        재생 스레드
// ===================================================================
void *playback_thread_func(void *arg) {
    int fd = open(vs10xx_dev_path, O_WRONLY);
    if (fd < 0) {
        perror("Playback Thread: Failed to open vs10xx device");
        return NULL;
    }

    while (keep_running_threads) {
        pthread_mutex_lock(&player_mutex);
        // "재생" 상태가 아니면 신호가 올 때까지 대기
        while (playback_state != STATE_PLAYING && keep_running_threads) {
            pthread_cond_wait(&player_cond, &player_mutex);
        }
        pthread_mutex_unlock(&player_mutex);

        if (!keep_running_threads) break;

        // --- 실제 파일 재생 로직 ---
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
            // 일시정지 상태면 대기
            while (playback_state == STATE_PAUSED && !request_track_change) {
                pthread_cond_wait(&player_cond, &player_mutex);
            }
            // 곡 변경 요청이 들어오면 즉시 재생 중단
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
        // -------------------------

        pthread_mutex_lock(&player_mutex);
        if (!request_track_change) { // 곡이 정상적으로 끝나면
            current_track = (current_track + 1) % num_tracks; // 다음 곡으로 자동 이동
        }
        pthread_mutex_unlock(&player_mutex);

        // 다음 곡 재생을 위해 제어 스레드에 신호 전송
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

    // 클릭 감지를 위한 변수
    int click_count = 0;
    struct timespec last_click_time = {0, 0};

    if (rotary_fd < 0 || vs10xx_fd < 0) { /* ... */ return NULL; }
    printf("Control thread started. Ready for input.\n");

    while (keep_running_threads) {
        lseek(rotary_fd, 0, SEEK_SET);
        ssize_t bytes = read(rotary_fd, read_buf, sizeof(read_buf) - 1);
        if (bytes > 0) {
            read_buf[bytes] = '\0';
            if (sscanf(read_buf, "%d,%d", &current_count, &key_event) == 2) {
                // 볼륨 조절
                if (current_count != last_count) {
                    unsigned char volume = map_count_to_volume(current_count);
                    unsigned int packed_volume = (volume << 8) | volume;
                    
                    if (ioctl(vs10xx_fd, VS10XX_SET_VOL, &packed_volume) == 0) {
                        printf("Rotary count: %d -> Volume set to: %d\n", current_count, volume);
                    }
                    last_count = current_count;
                }
                // 키 이벤트 감지
                if (key_event == 1) {
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    if (get_time_diff_ms(&last_click_time, &now) > CLICK_TIMEOUT_MS) {
                        click_count = 1; // 타임아웃 지났으면 새로 카운트
                    } else {
                        click_count++; // 연속 클릭
                    }
                    last_click_time = now;
                }
            }
        }

        // --- 클릭 타임아웃 처리 ---
        if (click_count > 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (get_time_diff_ms(&last_click_time, &now) > CLICK_TIMEOUT_MS) {
                pthread_mutex_lock(&player_mutex);
                if (click_count == 1) { // 싱글 클릭: 일시정지/재생
                    if (playback_state == STATE_PLAYING) {
                        playback_state = STATE_PAUSED;
                        printf("|| Paused\n");
                    } else {
                        playback_state = STATE_PLAYING;
                        printf("▶? Resumed\n");
                    }
                } else if (click_count == 2) { // 더블 클릭: 다음 곡
                    printf(">> Next Track\n");
                    request_track_change = 1;
                    current_track = (current_track + 1) % num_tracks;
                } else if (click_count >= 3) { // 트리플 클릭: 이전 곡
                    printf("<< Previous Track\n");
                    request_track_change = -1;
                    current_track = (current_track - 1 + num_tracks) % num_tracks;
                }
                
                // 재생 스레드에 상태 변경 알림
                pthread_cond_signal(&player_cond);
                pthread_mutex_unlock(&player_mutex);
                click_count = 0; // 처리 완료 후 초기화
            }
        }
        usleep(10000); // 0.01초마다 체크
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

    // 초기 볼륨 설정
    int initial_fd = open(vs10xx_dev_path, O_WRONLY);
    if(initial_fd > 0) {
        unsigned char vol = map_count_to_volume(0); // 카운트 0일때 볼륨
        unsigned int p_vol = (vol << 8) | vol;
        ioctl(initial_fd, VS10XX_SET_VOL, &p_vol);
        close(initial_fd);
    }
    
    // 스레드 생성
    pthread_create(&playback_thread_id, NULL, playback_thread_func, NULL);
    pthread_create(&control_thread_id, NULL, control_thread_func, NULL);

    // 프로그램 시작 시 자동 재생
    pthread_mutex_lock(&player_mutex);
    playback_state = STATE_PLAYING;
    pthread_cond_signal(&player_cond);
    pthread_mutex_unlock(&player_mutex);
    
    // 스레드가 끝날 때까지 대기
    pthread_join(playback_thread_id, NULL);
    pthread_join(control_thread_id, NULL);

    printf("Program terminated.\n");
    return 0;
}