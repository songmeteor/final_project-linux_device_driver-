#ifndef __OLED_UI_H__
#define __OLED_UI_H__

#define VISUALIZER_BARS 20 // �ð�ȭ ���� ����

// MP3 �÷��̾� UI �����͸� ��� ����ü
struct oled_mp3_ui_data {
    char song_title[21];    // �뷡 ����
    char current_time[6];   // "MM:SS"
    char total_time[6];     // "MM:SS"
    char track_info[8];     // "XXX/XXX"
    int play_state;         // 0: �Ͻ�����, 1: ���, 2: ����
    int progress;           // ����� (0-100)
    // ����� �ð�ȭ ������ (�� ������ ����, 0-16)
    unsigned char visualizer_bars[VISUALIZER_BARS]; 
};

// IOCTL ��ɾ� ����
#define OLED_IOCTL_MAGIC 'o'
#define OLED_UPDATE_UI _IOW(OLED_IOCTL_MAGIC, 1, struct oled_mp3_ui_data)

#endif // __OLED_UI_H__
