// oled_test.c (�ִϸ��̼� �׽�Ʈ ����)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>      // usleep �Լ��� ���� �߰�
#include "oled.h"

#define DEVICE_PATH "/dev/oled"

int main() {
    int fd;
    struct mp3_ui_data ui_data;
    int i; // �ݺ����� ���� ����

    // ����̽� ���� ����
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open the device file");
        return -1;
    }

    // --- �⺻ UI ������ ���� (�� ���� ����) ---
    memset(&ui_data, 0, sizeof(struct mp3_ui_data));
    ui_data.volume = 11;
    strncpy(ui_data.current_time, "12:04", sizeof(ui_data.current_time) - 1);
    ui_data.track_current = 1;
    ui_data.track_total = 5;
    strncpy(ui_data.playback_time, "00:32", sizeof(ui_data.playback_time) - 1);
    strncpy(ui_data.song_title, "Animation Test", sizeof(ui_data.song_title) - 1);
    strncpy(ui_data.total_time, "02:48", sizeof(ui_data.total_time) - 1);

    printf("Starting spectrum analyzer animation for 10 seconds...\n");

    // --- 100�� �ݺ��ϸ� �ִϸ��̼� ���� (100 * 0.1�� = 10��) ---
    for (i = 0; i < 100; i++) {
        // �Ź� ������ �����͸� ������, ����̹��� ���� ���� �ٽ� �����մϴ�.
        ssize_t bytes_written = write(fd, &ui_data, sizeof(struct mp3_ui_data));

        if (bytes_written < 0) {
            perror("Failed to write to the device");
            break; // ���� �߻� �� �ݺ� �ߴ�
        }

        // �͹̳ο� ���� ��Ȳ ���
        printf(".");
        fflush(stdout); // ���۸� ��� "."�� �ٷ� ���̵��� ��

        // 100,000 ����ũ���� (0.1��) ���� ���
        usleep(200000);
    }
    
    printf("\nAnimation test finished.\n");

    // ����̽� ���� �ݱ�
    close(fd);

    return 0;
}