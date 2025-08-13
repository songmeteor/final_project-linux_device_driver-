#ifndef OLED_H
#define OLED_H

// UI 데이터를 담는 구조체
struct mp3_ui_data {
    // 1) 볼륨 (0~15)
    unsigned char volume;

    // 2) 현재 시간 (HH:MM) - 예: "10:43"
    char current_time[6];

    // 3) 트랙 번호
    unsigned int track_current;
    unsigned int track_total;

    // 5) 곡 재생 시간 (MM:SS) - 예: "01:23"
    char playback_time[6];

    // 6) 곡 제목 (최대 20자)
    char song_title[21];

    // 7) 곡 전체 시간 (MM:SS) - 예: "03:45"
    char total_time[6];
};

#endif // OLED_H