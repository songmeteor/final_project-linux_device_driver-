#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/random.h>
#include <linux/slab.h>     // kmalloc, kfree를 위해 추가
#include <linux/delay.h>
#include "oled.h"

// --- I2C 및 화면 정보 ---
#define I2C_BUS_NUMBER  1   // 사용하는 I2C 버스 번호 (예: Raspberry Pi는 1번)
#define OLED_I2C_ADDR   0x3C  // OLED의 I2C 주소
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define SCREEN_PAGES    (SCREEN_HEIGHT / 8)

// --- 드라이버 정보 ---
#define DRIVER_NAME "oled"
#define DEVICE_NAME "oled"
#define CLASS_NAME  "oled_class"

// --- 전역 변수 ---
static dev_t dev_num;
static struct class *mp3_class;
static struct cdev mp3_cdev;
static struct i2c_client *oled_client;

// OLED 화면을 위한 프레임 버퍼
static unsigned char oled_buffer[SCREEN_WIDTH * SCREEN_PAGES];

extern const unsigned char font5x7[];
static const unsigned char icon_speaker[] = {
    0x18, 0x3C, 0x3C, 0x7E, 0xC3, 0xFF, 0xFF
};

// ===================================================================
// == OLED 제어 함수 (하드웨어 종속적인 부분) - 완성된 버전 ==
// ===================================================================

// I2C로 명령어 전송
static int oled_send_cmd(unsigned char cmd) {
    char buf[2] = {0x00, cmd}; // Control byte 0x00은 명령어임을 의미
    int ret;
    if (!oled_client) return -ENODEV;
    
    ret = i2c_master_send(oled_client, buf, 2);
    if (ret < 0) {
        printk(KERN_ERR "[OLED] i2c_master_send (CMD) failed: %d\n", ret);
    }
    return ret;
}

// 화면 버퍼를 OLED로 전송 (효율적인 방식으로 변경)
static void oled_flush_buffer(void) {
    // 1024바이트의 화면 데이터 + 1바이트의 컨트롤 바이트
    unsigned char *transfer_buf;
    int ret;

    if (!oled_client) return;

    transfer_buf = kmalloc(sizeof(oled_buffer) + 1, GFP_KERNEL);
    if (!transfer_buf) {
        printk(KERN_ERR "[OLED] Failed to allocate memory for flush buffer\n");
        return;
    }
    
    transfer_buf[0] = 0x40; // 첫 바이트는 데이터 전송을 의미하는 컨트롤 바이트
    memcpy(&transfer_buf[1], oled_buffer, sizeof(oled_buffer));

    // 화면 업데이트를 위한 주소 설정 명령어
    oled_send_cmd(0x21); // Set Column Address
    oled_send_cmd(0);    // Start
    oled_send_cmd(127);  // End
    oled_send_cmd(0x22); // Set Page Address
    oled_send_cmd(0);    // Start
    oled_send_cmd(7);    // End

    // 준비된 버퍼를 I2C를 통해 한번에 전송
    ret = i2c_master_send(oled_client, transfer_buf, sizeof(oled_buffer) + 1);
    if (ret < 0) {
        printk(KERN_ERR "[OLED] Failed to flush buffer to screen: %d\n", ret);
    } else {
        printk(KERN_INFO "[OLED] Buffer flushed to screen (%d bytes).\n", ret);
    }

    kfree(transfer_buf);
}

// OLED 컨트롤러 초기화 (SSD1306 기준)
static void oled_init_sequence(void)
{
    msleep(20); // 전원 안정화 대기
    oled_send_cmd(0xAE); // Display OFF
    oled_send_cmd(0xD5); // Set Display Clock Divide Ratio/Oscillator Frequency
    oled_send_cmd(0x80);
    oled_send_cmd(0xA8); // Set MUX Ratio
    oled_send_cmd(0x3F); // 64 MUX
    oled_send_cmd(0xD3); // Set Display Offset
    oled_send_cmd(0x00);
    oled_send_cmd(0x40); // Set Display Start Line
    oled_send_cmd(0x8D); // Charge Pump Setting
    oled_send_cmd(0x14); // Enable Charge Pump
    oled_send_cmd(0x20); // Set Memory Addressing Mode
    oled_send_cmd(0x00); // Horizontal Addressing Mode
    oled_send_cmd(0xA1); // Set Segment Re-map (column 127 mapped to SEG0)
    oled_send_cmd(0xC8); // Set COM Output Scan Direction (reversed)
    oled_send_cmd(0xDA); // Set COM Pins Hardware Configuration
    oled_send_cmd(0x12);
    oled_send_cmd(0x81); // Set Contrast Control
    oled_send_cmd(0xCF);
    oled_send_cmd(0xD9); // Set Pre-charge Period
    oled_send_cmd(0xF1);
    oled_send_cmd(0xDB); // Set VCOMH Deselect Level
    oled_send_cmd(0x40);
    oled_send_cmd(0xA4); // Entire Display ON from RAM
    oled_send_cmd(0xA6); // Set Normal Display
    oled_send_cmd(0xAF); // Display ON
    printk(KERN_INFO "[OLED] Controller initialized.\n");
}


// ===================================================================
// == 그래픽 함수 (변경 없음) ==
// ===================================================================
// 버퍼 초기화 (전부 0으로)
static void oled_clear_buffer(void) {
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}
// 특정 위치에 픽셀 그리기
static void oled_draw_pixel(int x, int y, int color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }
    if (color) {
        oled_buffer[x + (y / 8) * SCREEN_WIDTH] |= (1 << (y % 8));
    } else {
        oled_buffer[x + (y / 8) * SCREEN_WIDTH] &= ~(1 << (y % 8));
    }
}
// 사각형 그리기
static void oled_draw_rect(int x, int y, int w, int h, int fill) {
    int i, j;
    for (i = x; i < x + w; i++) {
        for (j = y; j < y + h; j++) {
            if (fill) {
                oled_draw_pixel(i, j, 1);
            } else { // 테두리만
                if (i == x || i == x + w - 1 || j == y || j == y + h - 1) {
                    oled_draw_pixel(i, j, 1);
                }
            }
        }
    }
}
// 문자열 출력
static void oled_draw_string(int x, int y, const char *str) {
    int i = 0;
    while (*str) {
        char c = *str++;
        int char_idx = c - ' '; // 폰트 데이터에서 문자 인덱스
        if (char_idx < 0 || char_idx > 95) continue; // 지원 안하는 문자

        int j, k;
        for (j = 0; j < 5; j++) {
            unsigned char line = font5x7[char_idx * 5 + j];
            for (k = 0; k < 8; k++) {
                if ((line >> k) & 1) {
                    oled_draw_pixel(x + i * 6 + j, y + k, 1);
                }
            }
        }
        i++;
    }
}

// ===================================================================
// == UI 요소 그리기 함수 (변경 없음) ==
// ===================================================================
// 1) 볼륨 그리기 (0~15 값을 5개 막대로)
static void draw_volume(int level) {
    int i;
    int bar_heights[] = {2, 4, 6, 8, 10}; // 5개 막대의 최대 높이
    int thresholds[] = {0, 4, 7, 10, 13}; // 각 막대가 켜지는 볼륨 레벨 임계값
    
    for (i = 0; i < 5; i++) {
        if (level >= thresholds[i]) {
            oled_draw_rect(10 + i * 4, 11 - bar_heights[i], 3, bar_heights[i], 1);
        }
    }
}
// 4) 스펙트럼 분석기 애니메이션 (랜덤 막대)
static void draw_spectrum_analyzer(void) {
    int i;
    for (i = 0; i < 32; i++) {
        unsigned int rand;
        get_random_bytes(&rand, sizeof(rand));
        int height = rand % 16; // 0~15 높이의 랜덤 막대
        oled_draw_rect(2 + i * 4, 38 - height, 3, height, 1);
    }
}

static void oled_draw_bitmap(int x, int y, int w, int h, const unsigned char *bitmap) {
    int i, j;
    // 비트맵의 모든 픽셀을 순회
    for (j = 0; j < h; j++) { // 세로 (y)
        for (i = 0; i < w; i++) { // 가로 (x)
            // 비트맵 데이터에서 현재 픽셀이 켜져 있는지 확인
            // (비트맵은 세로 8픽셀이 1바이트로 구성됨)
            if ( (bitmap[i] >> j) & 1 ) {
                // 켜져 있다면, 화면의 해당 위치에 픽셀을 그림
                oled_draw_pixel(x + i, y + j, 1);
            }
        }
    }
}

// UI 전체 업데이트
static void update_display(struct mp3_ui_data *data) {
    char temp_str[16];

    oled_clear_buffer();

    // 1) 볼륨
	oled_draw_bitmap(2, 2, 8, 8, icon_speaker);
    draw_volume(data->volume);
    // 2) 현재 시간
    oled_draw_string(48, 2, data->current_time);
    // 3) 트랙 번호
    snprintf(temp_str, sizeof(temp_str), "%02d/%02d", data->track_current, data->track_total);
    oled_draw_string(90, 2, temp_str);
    // 4) 스펙트럼 분석기
	if(!data->spectrum_run_stop) draw_spectrum_analyzer();
    // 5) 현재 재생 시간
    oled_draw_string(4, 54, data->playback_time);
    // 6) 곡 제목 (가운데 정렬)
    int title_len = strlen(data->song_title);
    int title_x = 64 - (title_len * 6) / 2;
    if (title_x < 0) title_x = 0;
    oled_draw_string(title_x, 44, data->song_title);
    // 7) 곡 전체 시간
    oled_draw_string(90, 54, data->total_time);

    // 완성된 버퍼를 화면에 전송
    oled_flush_buffer();

}

// ===================================================================
// == 파일 오퍼레이션 (File Operations) - 변경 없음 ==
// ===================================================================
static int mp3_oled_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "[OLED] Device opened.\n");
    return 0;
}
static int mp3_oled_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "[OLED] Device closed.\n");
    return 0;
}
static ssize_t mp3_oled_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offs) {
    struct mp3_ui_data data;
    
    if (count != sizeof(struct mp3_ui_data)) {
        printk(KERN_WARNING "[OLED] Write size mismatch! Expected %ld, got %ld\n", sizeof(struct mp3_ui_data), count);
        return -EINVAL;
    }
    
    if (copy_from_user(&data, user_buf, sizeof(struct mp3_ui_data))) {
        printk(KERN_ERR "[OLED] Failed to copy data from user.\n");
        return -EFAULT;
    }

    printk(KERN_INFO "[OLED] Received data: vol=%d, title=%s\n", data.volume, data.song_title);
    
    update_display(&data);
    
    return count;
}

static const struct file_operations mp3_oled_fops = {
    .owner = THIS_MODULE,
    .open = mp3_oled_open,
    .release = mp3_oled_release,
    .write = mp3_oled_write,
};

// ===================================================================
// == 드라이버 초기화 및 종료 - 완성된 버전 ==
// ===================================================================

static int __init mp3_oled_init(void) {
    struct i2c_adapter *adapter;
    struct i2c_board_info oled_info = {
        I2C_BOARD_INFO("oled", OLED_I2C_ADDR)
    };
    
    // 1. 캐릭터 디바이스 번호 할당
    if (alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME) < 0) {
        printk(KERN_ERR "[OLED] Failed to allocate device number.\n");
        return -1;
    }
    // 2. 디바이스 클래스 생성
    mp3_class = class_create(CLASS_NAME);
    if (IS_ERR(mp3_class)) {
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(mp3_class);
    }
    // 3. 디바이스 파일 생성
    if (device_create(mp3_class, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        class_destroy(mp3_class);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }
    // 4. 캐릭터 디바이스 초기화 및 등록
    cdev_init(&mp3_cdev, &mp3_oled_fops);
    if (cdev_add(&mp3_cdev, dev_num, 1) < 0) {
        device_destroy(mp3_class, dev_num);
        class_destroy(mp3_class);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    // 5. I2C 장치 설정
    adapter = i2c_get_adapter(I2C_BUS_NUMBER);
    if (!adapter) {
        printk(KERN_ERR "[OLED] Cannot get I2C adapter %d\n", I2C_BUS_NUMBER);
        // 에러 발생 시 생성했던 캐릭터 디바이스 정리
        cdev_del(&mp3_cdev);
        device_destroy(mp3_class, dev_num);
        class_destroy(mp3_class);
        unregister_chrdev_region(dev_num, 1);
        return -ENODEV;
    }

    oled_client = i2c_new_client_device(adapter, &oled_info);
    i2c_put_adapter(adapter);

    if (!oled_client) {
        printk(KERN_ERR "[OLED] Cannot create I2C client device at address 0x%x\n", oled_info.addr);
        // 에러 발생 시 생성했던 캐릭터 디바이스 정리
        cdev_del(&mp3_cdev);
        device_destroy(mp3_class, dev_num);
        class_destroy(mp3_class);
        unregister_chrdev_region(dev_num, 1);
        return -ENODEV;
    }
    
    // 6. OLED 컨트롤러 초기화
    oled_init_sequence();

    // 7. 모듈 로드 시 기본 화면 출력
    struct mp3_ui_data initial_data = {
        .volume = 0, .track_current = 0, .track_total = 0,
        .current_time = "00:00", .playback_time = "00:00", .total_time = "00:00",
        .song_title = "Initializing..."
    };
    update_display(&initial_data);

    printk(KERN_INFO "[OLED] Driver loaded successfully. Device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit mp3_oled_exit(void) {
    // 모듈 종료 시 화면 끄기
    oled_send_cmd(0xAE);

    if(oled_client) i2c_unregister_device(oled_client);

    cdev_del(&mp3_cdev);
    device_destroy(mp3_class, dev_num);
    class_destroy(mp3_class);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "[OLED] Driver unloaded.\n");
}

module_init(mp3_oled_init);
module_exit(mp3_oled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("MP3 Player UI Driver for 128x64 I2C OLED (Completed)");

// 폰트 데이터 (이전과 동일)
const unsigned char font5x7[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, /* Espace	0x20 */
	0x00, 0x00, 0x4f, 0x00, 0x00, /* ! */
	0x00, 0x07, 0x00, 0x07, 0x00, /* " */
	0x14, 0x7f, 0x14, 0x7f, 0x14, /* # */
	0x24, 0x2a, 0x7f, 0x2a, 0x12, /* $ */
	0x23, 0x13, 0x08, 0x64, 0x62, /* % */
	0x36, 0x49, 0x55, 0x22, 0x50, /* & */
	0x00, 0x05, 0x03, 0x00, 0x00, /* ' */
	0x00, 0x1c, 0x22, 0x41, 0x00, /* ( */
	0x00, 0x41, 0x22, 0x1c, 0x00, /* ) */
	0x08, 0x2a, 0x1c, 0x2a, 0x08, /* * */
	0x08, 0x08, 0x3e, 0x08, 0x08, /* + */
	0x00, 0x50, 0x30, 0x00, 0x00, /* , */
	0x08, 0x08, 0x08, 0x08, 0x08, /* - */
	0x00, 0x60, 0x60, 0x00, 0x00, /* . */
	0x20, 0x10, 0x08, 0x04, 0x02, /* / */
	0x3e, 0x51, 0x49, 0x45, 0x3e, /* 0 */
	0x00, 0x42, 0x7f, 0x40, 0x00, /* 1 */
	0x42, 0x61, 0x51, 0x49, 0x46, /* 2 */
	0x21, 0x41, 0x45, 0x4b, 0x31, /* 3 */
	0x18, 0x14, 0x12, 0x7f, 0x10, /* 4 */
	0x27, 0x45, 0x45, 0x45, 0x39, /* 5 */
	0x3c, 0x4a, 0x49, 0x49, 0x30, /* 6 */
	0x01, 0x71, 0x09, 0x05, 0x03, /* 7 */
	0x36, 0x49, 0x49, 0x49, 0x36, /* 8 */
	0x06, 0x49, 0x49, 0x29, 0x1e, /* 9 */
	0x00, 0x36, 0x36, 0x00, 0x00, /* : */
	0x00, 0x56, 0x36, 0x00, 0x00, /* ; */
	0x00, 0x08, 0x14, 0x22, 0x41, /* < */
	0x14, 0x14, 0x14, 0x14, 0x14, /* = */
	0x41, 0x22, 0x14, 0x08, 0x00, /* > */
	0x02, 0x01, 0x51, 0x09, 0x06, /* ? */
	0x32, 0x49, 0x79, 0x41, 0x3e, /* @ */
	0x7e, 0x11, 0x11, 0x11, 0x7e, /* A */
	0x7f, 0x49, 0x49, 0x49, 0x36, /* B */
	0x3e, 0x41, 0x41, 0x41, 0x22, /* C */
	0x7f, 0x41, 0x41, 0x22, 0x1c, /* D */
	0x7f, 0x49, 0x49, 0x49, 0x41, /* E */
	0x7f, 0x09, 0x09, 0x01, 0x01, /* F */
	0x3e, 0x41, 0x41, 0x51, 0x32, /* G */
	0x7f, 0x08, 0x08, 0x08, 0x7f, /* H */
	0x00, 0x41, 0x7f, 0x41, 0x00, /* I */
	0x20, 0x40, 0x41, 0x3f, 0x01, /* J */
	0x7f, 0x08, 0x14, 0x22, 0x41, /* K */
	0x7f, 0x40, 0x40, 0x40, 0x40, /* L */
	0x7f, 0x02, 0x04, 0x02, 0x7f, /* M */
	0x7f, 0x04, 0x08, 0x10, 0x7f, /* N */
	0x3e, 0x41, 0x41, 0x41, 0x3e, /* O */
	0x7f, 0x09, 0x09, 0x09, 0x06, /* P */
	0x3e, 0x41, 0x51, 0x21, 0x5e, /* Q */
	0x7f, 0x09, 0x19, 0x29, 0x46, /* R */
	0x46, 0x49, 0x49, 0x49, 0x31, /* S */
	0x01, 0x01, 0x7f, 0x01, 0x01, /* T */
	0x3f, 0x40, 0x40, 0x40, 0x3f, /* U */
	0x1f, 0x20, 0x40, 0x20, 0x1f, /* V */
	0x3f, 0x40, 0x38, 0x40, 0x3f, /* W */
	0x63, 0x14, 0x08, 0x14, 0x63, /* X */
	0x03, 0x04, 0x78, 0x04, 0x03, /* Y */
	0x61, 0x51, 0x49, 0x45, 0x43, /* Z */
	0x00, 0x00, 0x7f, 0x41, 0x41, /* [ */
	0x02, 0x04, 0x08, 0x10, 0x20, /* \ */
	0x41, 0x41, 0x7f, 0x00, 0x00, /* ] */
	0x04, 0x02, 0x01, 0x02, 0x04, /* ^ */
	0x40, 0x40, 0x40, 0x40, 0x40, /* _ */
	0x00, 0x01, 0x02, 0x04, 0x00, /* ` */
	0x20, 0x54, 0x54, 0x54, 0x78, /* a */
	0x7f, 0x48, 0x44, 0x44, 0x38, /* b */
	0x38, 0x44, 0x44, 0x44, 0x20, /* c */
	0x38, 0x44, 0x44, 0x48, 0x7f, /* d */
	0x38, 0x54, 0x54, 0x54, 0x18, /* e */
	0x08, 0x7e, 0x09, 0x01, 0x02, /* f */
	0x0C, 0x52, 0x52, 0x52, 0x3E, /* g */
	0x7f, 0x08, 0x04, 0x04, 0x78, /* h */
	0x00, 0x44, 0x7d, 0x40, 0x00, /* i */
	0x20, 0x40, 0x44, 0x3d, 0x00, /* j */
	0x00, 0x7f, 0x10, 0x28, 0x44, /* k */
	0x00, 0x41, 0x7f, 0x40, 0x00, /* l */
	0x7c, 0x04, 0x18, 0x04, 0x78, /* m */
	0x7c, 0x08, 0x04, 0x04, 0x78, /* n */
	0x38, 0x44, 0x44, 0x44, 0x38, /* o */
	0x7c, 0x14, 0x14, 0x14, 0x08, /* p */
	0x08, 0x14, 0x14, 0x18, 0x7c, /* q */
	0x7c, 0x08, 0x04, 0x04, 0x08, /* r */
	0x48, 0x54, 0x54, 0x54, 0x20, /* s */
	0x04, 0x3f, 0x44, 0x40, 0x20, /* t */
	0x3c, 0x40, 0x40, 0x20, 0x7c, /* u */
	0x1c, 0x20, 0x40, 0x20, 0x1c, /* v */
	0x3c, 0x40, 0x30, 0x40, 0x3c, /* w */
	0x44, 0x28, 0x10, 0x28, 0x44, /* x */
	0x0c, 0x50, 0x50, 0x50, 0x3c, /* y */
	0x44, 0x64, 0x54, 0x4c, 0x44, /* z */
	0x00, 0x08, 0x36, 0x41, 0x00, /* { */
	0x00, 0x00, 0x7f, 0x00, 0x00, /* | */
	0x00, 0x41, 0x36, 0x08, 0x00, /* } */
	0x08, 0x04, 0x08, 0x10, 0x08, /* ~ */
};