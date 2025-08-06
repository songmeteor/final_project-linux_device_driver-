#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/oled_dev"

int main(int argc, char *argv[]) {
    int fd;
    char *write_buf;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message to display>\n", argv[0]);
        fprintf(stderr, "       %s -c (to clear screen)\n", argv[0]);
        return 1;
    }

    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device file");
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0) {
        // ȭ���� ����� ���� �� �ǵ� ���ڸ� ����
        write_buf = "\f";
        write(fd, write_buf, 1);
        printf("Clear command sent to OLED.\n");
    } else {
        write_buf = argv[1];
        int len = strlen(write_buf);
        if (write(fd, write_buf, len) != len) {
            perror("Failed to write to device");
        } else {
            printf("Message sent to OLED: %s\n", write_buf);
        }
    }

    close(fd);
    return 0;
}