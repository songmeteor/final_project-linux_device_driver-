CC=gcc
CFLAGS=-Wall -Wextra -O2
LDFLAGS=-lpthread  # <-- 스레드 라이브러리 링크 플래그 추가
TARGET=ioctl_vol_ctrl

all: $(TARGET)

$(TARGET): ioctl_vol_ctrl.c vs10xx.h
	$(CC) $(CFLAGS) -o $(TARGET) ioctl_vol_ctrl.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
