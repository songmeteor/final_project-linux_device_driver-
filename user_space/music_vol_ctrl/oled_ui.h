#ifndef __OLED_UI_H__
#define __OLED_UI_H__

#define VISUALIZER_BARS 20 // 시각화 막대 개수

// MP3 플레이어 UI 데이터를 담는 구조체
struct oled_mp3_ui_data {
    char song_title[21];    // 노래 제목
    char current_time[6];   // "MM:SS"
    char total_time[6];     // "MM:SS"
    char track_info[8];     // "XXX/XXX"
    int play_state;         // 0: 일시정지, 1: 재생, 2: 정지
    int progress;           // 진행률 (0-100)
    // 오디오 시각화 데이터 (각 막대의 높이, 0-16)
    unsigned char visualizer_bars[VISUALIZER_BARS]; 
};

// IOCTL 명령어 정의
#define OLED_IOCTL_MAGIC 'o'
#define OLED_UPDATE_UI _IOW(OLED_IOCTL_MAGIC, 1, struct oled_mp3_ui_data)

#endif // __OLED_UI_H__
