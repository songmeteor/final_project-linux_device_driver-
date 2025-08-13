#ifndef OLED_H
#define OLED_H

// UI �����͸� ��� ����ü
struct mp3_ui_data {
    // 1) ���� (0~15)
    unsigned char volume;

    // 2) ���� �ð� (HH:MM) - ��: "10:43"
    char current_time[6];

    // 3) Ʈ�� ��ȣ
    unsigned int track_current;
    unsigned int track_total;

    // 5) �� ��� �ð� (MM:SS) - ��: "01:23"
    char playback_time[6];

    // 6) �� ���� (�ִ� 20��)
    char song_title[21];

    // 7) �� ��ü �ð� (MM:SS) - ��: "03:45"
    char total_time[6];
};

#endif // OLED_H