#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "oled_ui.h"

#define OLED_DEVICE_NAME "oled_dev"
#define OLED_CLASS_NAME "oled_class"
#define OLED_I2C_ADDR 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)

#define FONT_WIDTH 5

static struct i2c_client *oled_client;
static struct cdev oled_cdev;
static dev_t oled_dev_num;
static struct class *oled_class;
static struct device *oled_device;

static unsigned char screen_buffer[OLED_WIDTH * OLED_PAGES];

// --- ��Ʈ �� ������ ������ ---
static const unsigned char font5x7[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x07,
	0x00, 0x07, 0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x24, 0x2A, 0x7F, 0x2A,
	0x12, 0x23, 0x13, 0x08, 0x64, 0x62, 0x36, 0x49, 0x55, 0x22, 0x50, 0x00,
	0x05, 0x03, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x41, 0x22,
	0x1C, 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x08, 0x08, 0x3E, 0x08, 0x08,
	0x00, 0x50, 0x30, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x60,
	0x60, 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x51, 0x49, 0x45,
	0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x42, 0x61, 0x51, 0x49, 0x46, 0x21,
	0x41, 0x45, 0x4B, 0x31, 0x18, 0x14, 0x12, 0x7F, 0x10, 0x27, 0x45, 0x45,
	0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x01, 0x71, 0x09, 0x05, 0x03,
	0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x36,
	0x36, 0x00, 0x00, 0x00, 0x56, 0x36, 0x00, 0x00, 0x08, 0x14, 0x22, 0x41,
	0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x41, 0x22, 0x14, 0x08, 0x02,
	0x01, 0x51, 0x09, 0x06, 0x32, 0x49, 0x79, 0x41, 0x3E, 0x7E, 0x11, 0x11,
	0x11, 0x7E, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E, 0x41, 0x41, 0x41, 0x22,
	0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09,
	0x09, 0x09, 0x01, 0x3E, 0x41, 0x49, 0x49, 0x7A, 0x7F, 0x08, 0x08, 0x08,
	0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F,
	0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40, 0x40, 0x7F, 0x02, 0x0C,
	0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
	0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09,
	0x19, 0x29, 0x46, 0x46, 0x49, 0x49, 0x49, 0x31, 0x01, 0x01, 0x7F, 0x01,
	0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x3F,
	0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14, 0x63, 0x07, 0x08, 0x70,
	0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x00,
	0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41, 0x7F, 0x00, 0x04, 0x02,
	0x01, 0x02, 0x04, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 0x02, 0x04,
	0x00, 0x20, 0x54, 0x54, 0x54, 0x78, 0x7F, 0x48, 0x44, 0x44, 0x38, 0x38,
	0x44, 0x44, 0x44, 0x20, 0x38, 0x44, 0x44, 0x48, 0x7F, 0x38, 0x54, 0x54,
	0x54, 0x18, 0x08, 0x7E, 0x09, 0x01, 0x02, 0x0C, 0x52, 0x52, 0x52, 0x3E,
	0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D, 0x40, 0x00, 0x20, 0x40,
	0x44, 0x3D, 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x41, 0x7F, 0x40,
	0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, 0x7C, 0x08, 0x04, 0x04, 0x78, 0x38,
	0x44, 0x44, 0x44, 0x38, 0x7C, 0x14, 0x14, 0x14, 0x08, 0x08, 0x14, 0x14,
	0x18, 0x7C, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48, 0x54, 0x54, 0x54, 0x20,
	0x04, 0x3F, 0x44, 0x40, 0x20, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x1C, 0x20,
	0x40, 0x20, 0x1C, 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x44, 0x28, 0x10, 0x28,
	0x44, 0x0C, 0x50, 0x50, 0x50, 0x3C, 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00,
	0x08, 0x36, 0x41, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x41, 0x36,
	0x08, 0x00, 0x02, 0x01, 0x02, 0x01, 0x00
};
static const unsigned char icon_prev[] = { 0x1C, 0x3C, 0x7C, 0xFC,
					   0xFC, 0x7C, 0x3C, 0x1C };
static const unsigned char icon_play[] = { 0x18, 0x3C, 0x7E, 0xFF,
					   0xFF, 0x7E, 0x3C, 0x18 };
static const unsigned char icon_pause[] = { 0xFF, 0xFF, 0xDB, 0xDB,
					    0xDB, 0xDB, 0xFF, 0xFF };
static const unsigned char icon_next[] = { 0x38, 0x3C, 0x3E, 0x3F,
					   0x3F, 0x3E, 0x3C, 0x38 };
static const unsigned char icon_repeat[] = { 0x0C, 0x1E, 0x33, 0x21,
					     0x42, 0x66, 0x3C, 0x18 };

// --- I2C �� ȭ�� ���� �Լ� ---
static int oled_send_cmd(u8 cmd)
{
	return i2c_smbus_write_byte_data(oled_client, 0x00, cmd);
}
static void oled_set_pos(int x, int page)
{
	oled_send_cmd(0xB0 + page);
	oled_send_cmd(0x00 + (x & 0x0F));
	oled_send_cmd(0x10 + (x >> 4));
}
static void oled_refresh_screen(void)
{
	int i;
	oled_set_pos(0, 0);
	for (i = 0; i < sizeof(screen_buffer); i++) {
		i2c_smbus_write_byte_data(oled_client, 0x40, screen_buffer[i]);
	}
}

// --- �����ӹ��ۿ� �׸��� �Լ��� ---
static void draw_char(int x, int y, char c)
{
	int i;
	const unsigned char *font =
		&font5x7[(unsigned char)(c - 32) * FONT_WIDTH];
	if (x > OLED_WIDTH - FONT_WIDTH || y > OLED_HEIGHT - 8)
		return;
	for (i = 0; i < FONT_WIDTH; i++) {
		screen_buffer[(y / 8) * OLED_WIDTH + x + i] = font[i];
	}
}
static void draw_string(int x, int y, const char *str)
{
	while (*str) {
		draw_char(x, y, *str++);
		x += FONT_WIDTH + 1;
	}
}
static void draw_icon(int x, int y, const unsigned char *icon)
{
	int i;
	if (x > OLED_WIDTH - 8 || y > OLED_HEIGHT - 8)
		return;
	for (i = 0; i < 8; i++) {
		screen_buffer[(y / 8) * OLED_WIDTH + x + i] = icon[i];
	}
}
static void draw_visualizer(int x, int y, const unsigned char *bars)
{
	int i, j;
	for (i = 0; i < VISUALIZER_BARS; i++) {
		for (j = 0; j < 16; j++) { // Max height
			int page = (y + j) / 8;
			int bit_pos = (y + j) % 8;
			int idx = page * OLED_WIDTH + (x + i * 4);
			if (j < bars[i]) {
				screen_buffer[idx] |= (1 << bit_pos);
				screen_buffer[idx + 1] |= (1 << bit_pos);
			} else {
				screen_buffer[idx] &= ~(1 << bit_pos);
				screen_buffer[idx + 1] &= ~(1 << bit_pos);
			}
		}
	}
}
static void draw_progress_bar(int y, int progress)
{
	int i;
	int bar_width = (OLED_WIDTH * progress) / 100;
	int page = y / 8;
	int bit_pos = y % 8;
	for (i = 0; i < OLED_WIDTH; i++) {
		int idx = page * OLED_WIDTH + i;
		if (i < bar_width) {
			screen_buffer[idx] |= (1 << bit_pos);
		} else {
			screen_buffer[idx] &= ~(1 << bit_pos);
		}
	}
}

// --- ����̹� �ٽ� �Լ� ---
static void oled_display_init(void)
{
	msleep(100);
	oled_send_cmd(0xAE);
	oled_send_cmd(0xD5);
	oled_send_cmd(0x80);
	oled_send_cmd(0xA8);
	oled_send_cmd(OLED_HEIGHT - 1);
	oled_send_cmd(0xD3);
	oled_send_cmd(0x00);
	oled_send_cmd(0x40 | 0x0);
	oled_send_cmd(0x8D);
	oled_send_cmd(0x14);
	oled_send_cmd(0x20);
	oled_send_cmd(0x00);
	oled_send_cmd(0xA1);
	oled_send_cmd(0xC8);
	oled_send_cmd(0xDA);
	oled_send_cmd(0x12);
	oled_send_cmd(0x81);
	oled_send_cmd(0xCF);
	oled_send_cmd(0xD9);
	oled_send_cmd(0xF1);
	oled_send_cmd(0xDB);
	oled_send_cmd(0x40);
	oled_send_cmd(0xA4);
	oled_send_cmd(0xA6);
	oled_send_cmd(0xAF);
	memset(screen_buffer, 0, sizeof(screen_buffer));
	oled_refresh_screen();
}

static long oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct oled_mp3_ui_data ui_data;

	if (cmd == OLED_UPDATE_UI) {
		if (copy_from_user(&ui_data, (void __user *)arg,
				   sizeof(ui_data)))
			return -EFAULT;

		memset(screen_buffer, 0, sizeof(screen_buffer));

		// // Row 2: Repeat Icon, Track Info
		// draw_icon(2, 8, icon_repeat);
		// draw_string(90, 8, ui_data.track_info);

		// Row 3: Visualizer
		draw_visualizer(24, 18, ui_data.visualizer_bars);

		// // Row 4: Time, Song Title
		// draw_string(2, 40, ui_data.current_time);
		// draw_string(40, 40, ui_data.song_title);
		// draw_string(98, 40, ui_data.total_time);

		// // Row 5: Progress Bar
		// draw_progress_bar(52, ui_data.progress);

		// // Row 6: Control Icons
		// draw_icon(30, 56, icon_prev);
		// if (ui_data.play_state == 1)
		// 	draw_icon(60, 56, icon_play);
		// else
		// 	draw_icon(60, 56, icon_pause);
		// draw_icon(90, 56, icon_next);

		oled_refresh_screen();
		return 0;
	}
	return -EINVAL;
}

static const struct file_operations oled_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = oled_ioctl,
};

static int __init oled_driver_init(void)
{
	struct i2c_adapter *adapter;
	struct i2c_board_info board_info = { .type = "ssd1306",
					     .addr = OLED_I2C_ADDR };
	if (alloc_chrdev_region(&oled_dev_num, 0, 1, OLED_DEVICE_NAME) < 0)
		return -1;
	cdev_init(&oled_cdev, &oled_fops);
	if (cdev_add(&oled_cdev, oled_dev_num, 1) < 0)
		goto unreg_chrdev;
	oled_class = class_create(OLED_CLASS_NAME);
	if (IS_ERR(oled_class))
		goto del_cdev;
	oled_device = device_create(oled_class, NULL, oled_dev_num, NULL,
				    OLED_DEVICE_NAME);
	if (IS_ERR(oled_device))
		goto destroy_class;
	adapter = i2c_get_adapter(1);
	if (!adapter)
		goto destroy_device;
	oled_client = i2c_new_client_device(adapter, &board_info);
	i2c_put_adapter(adapter);
	if (!oled_client)
		goto destroy_device;
	oled_display_init();
	pr_info("OLED MP3 UI driver initialized\n");
	return 0;

destroy_device:
	device_destroy(oled_class, oled_dev_num);
destroy_class:
	class_destroy(oled_class);
del_cdev:
	cdev_del(&oled_cdev);
unreg_chrdev:
	unregister_chrdev_region(oled_dev_num, 1);
	return -1;
}

static void __exit oled_driver_exit(void)
{
	oled_send_cmd(0xAE);
	i2c_unregister_device(oled_client);
	device_destroy(oled_class, oled_dev_num);
	class_destroy(oled_class);
	cdev_del(&oled_cdev);
	unregister_chrdev_region(oled_dev_num, 1);
	pr_info("OLED MP3 UI driver exited\n");
}

module_init(oled_driver_init);
module_exit(oled_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("OLED MP3 Player UI Driver");

// #include <linux/module.h>
// #include <linux/fs.h>
// #include <linux/cdev.h>
// #include <linux/device.h>
// #include <linux/uaccess.h>
// #include <linux/i2c.h>
// #include <linux/slab.h> // for kmalloc/kfree
// #include <linux/delay.h> // for msleep

// // --- OLED �� ����̽� ���� ---
// #define OLED_DEVICE_NAME "oled_dev"
// #define OLED_CLASS_NAME  "oled_class"
// #define OLED_I2C_ADDR    0x3C // i2cdetect -y 1 �� Ȯ�ε� �ּ�

// // --- OLED ���� ��� (SSD1306 ����) ---
// #define OLED_WIDTH 128
// #define OLED_HEIGHT 64
// #define OLED_PAGES (OLED_HEIGHT / 8)
// #define FONT_WIDTH 5
// #define FONT_HEIGHT 7

// #define OLED_CMD_SET_DISPLAY_OFF 0xAE
// #define OLED_CMD_SET_DISPLAY_ON  0xAF
// #define OLED_CMD_SET_CONTRAST    0x81
// #define OLED_CMD_ENTIRE_DISPLAY_ON 0xA4
// #define OLED_CMD_NORMAL_DISPLAY 0xA6
// #define OLED_CMD_SET_MEM_ADDR_MODE 0x20
// #define OLED_CMD_SET_COL_ADDR    0x21
// #define OLED_CMD_SET_PAGE_ADDR   0x22

// // --- ���� ���� ---
// static struct i2c_client *oled_client;
// static struct cdev oled_cdev;
// static dev_t oled_dev_num;
// static struct class *oled_class;
// static struct device *oled_device;

// // --- ���� Ŀ�� ��ġ ---
// static int cursor_x = 0;
// static int cursor_y = 0;

// // --- 5x7 ��Ʈ ������ (ASCII 32-126) ---
// static const unsigned char font5x7[] = {
// 	0x00, 0x00, 0x00, 0x00, 0x00, // sp  //0
// 	0x00, 0x00, 0x5F, 0x00, 0x00, // !
// 	0x00, 0x07, 0x00, 0x07, 0x00, // "
// 	0x14, 0x7F, 0x14, 0x7F, 0x14, // #
// 	0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
// 	0x23, 0x13, 0x08, 0x64, 0x62, // %
// 	0x36, 0x49, 0x55, 0x22, 0x50, // &
// 	0x00, 0x05, 0x03, 0x00, 0x00, // '
// 	0x00, 0x1C, 0x22, 0x41, 0x00, // (
// 	0x00, 0x41, 0x22, 0x1C, 0x00, // )
// 	0x14, 0x08, 0x3E, 0x08, 0x14, // *    //10
// 	0x08, 0x08, 0x3E, 0x08, 0x08, // +
// 	0x00, 0x50, 0x30, 0x00, 0x00, // ,
// 	0x08, 0x08, 0x08, 0x08, 0x08, // -
// 	0x00, 0x60, 0x60, 0x00, 0x00, // .
// 	0x20, 0x10, 0x08, 0x04, 0x02, // /
// 	0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
// 	0x00, 0x42, 0x7F, 0x40, 0x00, // 1
// 	0x42, 0x61, 0x51, 0x49, 0x46, // 2
// 	0x21, 0x41, 0x45, 0x4B, 0x31, // 3
// 	0x18, 0x14, 0x12, 0x7F, 0x10, // 4    //20
// 	0x27, 0x45, 0x45, 0x45, 0x39, // 5
// 	0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
// 	0x01, 0x71, 0x09, 0x05, 0x03, // 7
// 	0x36, 0x49, 0x49, 0x49, 0x36, // 8
// 	0x06, 0x49, 0x49, 0x29, 0x1E, // 9
// 	0x00, 0x36, 0x36, 0x00, 0x00, // :
// 	0x00, 0x56, 0x36, 0x00, 0x00, // ;
// 	0x08, 0x14, 0x22, 0x41, 0x00, // <
// 	0x14, 0x14, 0x14, 0x14, 0x14, // =
// 	0x00, 0x41, 0x22, 0x14, 0x08, // >   //30
// 	0x02, 0x01, 0x51, 0x09, 0x06, // ?
// 	0x32, 0x49, 0x79, 0x41, 0x3E, // @
// 	0x7E, 0x11, 0x11, 0x11, 0x7E, // A
// 	0x7F, 0x49, 0x49, 0x49, 0x36, // B
// 	0x3E, 0x41, 0x41, 0x41, 0x22, // C
// 	0x7F, 0x41, 0x41, 0x22, 0x1C, // D
// 	0x7F, 0x49, 0x49, 0x49, 0x41, // E
// 	0x7F, 0x09, 0x09, 0x09, 0x01, // F
// 	0x3E, 0x41, 0x49, 0x49, 0x7A, // G
// 	0x7F, 0x08, 0x08, 0x08, 0x7F, // H    //40
// 	0x00, 0x41, 0x7F, 0x41, 0x00, // I
// 	0x20, 0x40, 0x41, 0x3F, 0x01, // J
// 	0x7F, 0x08, 0x14, 0x22, 0x41, // K
// 	0x7F, 0x40, 0x40, 0x40, 0x40, // L
// 	0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
// 	0x7F, 0x04, 0x08, 0x10, 0x7F, // N
// 	0x3E, 0x41, 0x41, 0x41, 0x3E, // O
// 	0x7F, 0x09, 0x09, 0x09, 0x06, // P
// 	0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
// 	0x7F, 0x09, 0x19, 0x29, 0x46, // R
// 	0x46, 0x49, 0x49, 0x49, 0x31, // S
// 	0x01, 0x01, 0x7F, 0x01, 0x01, // T
// 	0x3F, 0x40, 0x40, 0x40, 0x3F, // U
// 	0x1F, 0x20, 0x40, 0x20, 0x1F, // V
// 	0x3F, 0x40, 0x38, 0x40, 0x3F, // W
// 	0x63, 0x14, 0x08, 0x14, 0x63, // X
// 	0x07, 0x08, 0x70, 0x08, 0x07, // Y
// 	0x61, 0x51, 0x49, 0x45, 0x43, // Z
// 	0x00, 0x7F, 0x41, 0x41, 0x00, // [
// 	0x02, 0x04, 0x08, 0x10, 0x20, // '\'
// 	0x00, 0x41, 0x41, 0x7F, 0x00, // ]
// 	0x04, 0x02, 0x01, 0x02, 0x04, // ^
// 	0x40, 0x40, 0x40, 0x40, 0x40, // _
// 	0x00, 0x01, 0x02, 0x04, 0x00, // `
// 	0x20, 0x54, 0x54, 0x54, 0x78, // a
// 	0x7F, 0x48, 0x44, 0x44, 0x38, // b
// 	0x38, 0x44, 0x44, 0x44, 0x20, // c
// 	0x38, 0x44, 0x44, 0x48, 0x7F, // d
// 	0x38, 0x54, 0x54, 0x54, 0x18, // e
// 	0x08, 0x7E, 0x09, 0x01, 0x02, // f
// 	0x0C, 0x52, 0x52, 0x52, 0x3E, // g
// 	0x7F, 0x08, 0x04, 0x04, 0x78, // h
// 	0x00, 0x44, 0x7D, 0x40, 0x00, // i
// 	0x20, 0x40, 0x44, 0x3D, 0x00, // j
// 	0x7F, 0x10, 0x28, 0x44, 0x00, // k
// 	0x00, 0x41, 0x7F, 0x40, 0x00, // l
// 	0x7C, 0x04, 0x18, 0x04, 0x78, // m
// 	0x7C, 0x08, 0x04, 0x04, 0x78, // n
// 	0x38, 0x44, 0x44, 0x44, 0x38, // o
// 	0x7C, 0x14, 0x14, 0x14, 0x08, // p
// 	0x08, 0x14, 0x14, 0x18, 0x7C, // q
// 	0x7C, 0x08, 0x04, 0x04, 0x08, // r
// 	0x48, 0x54, 0x54, 0x54, 0x20, // s
// 	0x04, 0x3F, 0x44, 0x40, 0x20, // t
// 	0x3C, 0x40, 0x40, 0x20, 0x7C, // u
// 	0x1C, 0x20, 0x40, 0x20, 0x1C, // v
// 	0x3C, 0x40, 0x30, 0x40, 0x3C, // w
// 	0x44, 0x28, 0x10, 0x28, 0x44, // x
// 	0x0C, 0x50, 0x50, 0x50, 0x3C, // y
// 	0x44, 0x64, 0x54, 0x4C, 0x44, // z
// 	0x00, 0x08, 0x36, 0x41, 0x00, // {
// 	0x00, 0x00, 0x7F, 0x00, 0x00, // |
// 	0x00, 0x41, 0x36, 0x08, 0x00, // }
// 	0x02, 0x01, 0x02, 0x01, 0x00, // ~
// };

// // --- I2C ��� �Լ� ---
// static int oled_send_cmd(u8 cmd) {
//     return i2c_smbus_write_byte_data(oled_client, 0x00, cmd);
// }

// static int oled_send_data(u8 data) {
//     return i2c_smbus_write_byte_data(oled_client, 0x40, data);
// }

// // --- OLED ȭ�� ���� �Լ� ---
// static void oled_set_cursor(int x, int y) {
//     oled_send_cmd(0xB0 + y); // Page
//     oled_send_cmd(0x00 + (x & 0x0F)); // Lower nibble of column
//     oled_send_cmd(0x10 + (x >> 4)); // Upper nibble of column
// }

// static void oled_clear(void) {
//     int i;
//     oled_set_cursor(0, 0);
//     for (i = 0; i < OLED_WIDTH * OLED_PAGES; i++) {
//         oled_send_data(0x00);
//     }
//     oled_set_cursor(0, 0);
//     cursor_x = 0;
//     cursor_y = 0;
// }

// static void oled_putc(char c) {
//     int i;

//     if (c == '\n') { // �ٹٲ�
//         cursor_x = 0;
//         cursor_y++;
//         if (cursor_y >= OLED_PAGES) {
//             cursor_y = 0;
//         }
//         oled_set_cursor(cursor_x, cursor_y);
//         return;
//     }

//     if (c == '\f') { // ȭ�� �����
//         oled_clear();
//         return;
//     }

//     if (cursor_x + FONT_WIDTH >= OLED_WIDTH) { // �ڵ� �ٹٲ�
//         cursor_x = 0;
//         cursor_y++;
//         if (cursor_y >= OLED_PAGES) {
//             cursor_y = 0;
//         }
//         oled_set_cursor(cursor_x, cursor_y);
//     }

//     // ��Ʈ �����Ϳ��� ���ڿ� �ش��ϴ� ��Ʈ���� ã�� ���
//     for (i = 0; i < FONT_WIDTH; i++) {
//         oled_send_data(font5x7[(c - 32) * FONT_WIDTH + i]);
//     }
//     // ���� ���̿� �� ĭ ���� �߰�
//     oled_send_data(0x00);
//     cursor_x += FONT_WIDTH + 1;
// }

// static void oled_display_init(void) {
//     msleep(100);
//     oled_send_cmd(OLED_CMD_SET_DISPLAY_OFF);
//     oled_send_cmd(0xD5); // Set Display Clock Divide Ratio
//     oled_send_cmd(0x80);
//     oled_send_cmd(0xA8); // Set Multiplex Ratio
//     oled_send_cmd(OLED_HEIGHT - 1);
//     oled_send_cmd(0xD3); // Set Display Offset
//     oled_send_cmd(0x00);
//     oled_send_cmd(0x40 | 0x0); // Set Start Line
//     oled_send_cmd(0x8D); // Charge Pump Setting
//     oled_send_cmd(0x14); // Enable Charge Pump
//     oled_send_cmd(OLED_CMD_SET_MEM_ADDR_MODE);
//     oled_send_cmd(0x00); // Horizontal Addressing Mode
//     oled_send_cmd(0xA1); // Set Segment Re-map
//     oled_send_cmd(0xC8); // Set COM Output Scan Direction
//     oled_send_cmd(0xDA); // Set COM Pins Hardware Configuration
//     oled_send_cmd(0x12);
//     oled_send_cmd(OLED_CMD_SET_CONTRAST);
//     oled_send_cmd(0xCF);
//     oled_send_cmd(0xD9); // Set Pre-charge Period
//     oled_send_cmd(0xF1);
//     oled_send_cmd(0xDB); // Set VCOMH Deselect Level
//     oled_send_cmd(0x40);
//     oled_send_cmd(OLED_CMD_ENTIRE_DISPLAY_ON);
//     oled_send_cmd(OLED_CMD_NORMAL_DISPLAY);
//     oled_send_cmd(OLED_CMD_SET_DISPLAY_ON);
//     oled_clear();
// }

// // --- ���� ���۷��̼� �Լ��� ---
// static int oled_open(struct inode *inode, struct file *file) {
//     pr_info("OLED driver: open() called\n");
//     return 0;
// }

// static int oled_release(struct inode *inode, struct file *file) {
//     pr_info("OLED driver: release() called\n");
//     return 0;
// }

// // write �ý��� ���� ȣ��� �� ����Ǵ� �Լ�
// static ssize_t oled_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
//     char *k_buf;
//     int i;

//     k_buf = kmalloc(len, GFP_KERNEL);
//     if (!k_buf) {
//         return -ENOMEM;
//     }

//     if (copy_from_user(k_buf, buf, len)) {
//         kfree(k_buf);
//         return -EFAULT;
//     }

//     // ���� ���ڿ��� �� ���ھ� OLED�� ���
//     for (i = 0; i < len; i++) {
//         oled_putc(k_buf[i]);
//     }

//     kfree(k_buf);
//     return len;
// }

// static const struct file_operations oled_fops = {
//     .owner   = THIS_MODULE,
//     .open    = oled_open,
//     .release = oled_release,
//     .write   = oled_write,
// };

// // --- ��� �ʱ�ȭ �� ���� �Լ� ---
// static int __init oled_driver_init(void) {
//     struct i2c_adapter *adapter;
//     struct i2c_board_info board_info = {
//         .type = "ssd1306",
//         .addr = OLED_I2C_ADDR,
//     };

//     // 1. ĳ���� ����̽� ���
//     if (alloc_chrdev_region(&oled_dev_num, 0, 1, OLED_DEVICE_NAME) < 0) {
//         pr_err("Failed to allocate device number\n");
//         return -1;
//     }
//     pr_info("Device number allocated: Major %d, Minor %d\n", MAJOR(oled_dev_num), MINOR(oled_dev_num));

//     // 2. cdev ����ü �ʱ�ȭ �� ���
//     cdev_init(&oled_cdev, &oled_fops);
//     if (cdev_add(&oled_cdev, oled_dev_num, 1) < 0) {
//         pr_err("Failed to add cdev\n");
//         goto unreg_chrdev;
//     }

//     // 3. ����̽� Ŭ���� ����
//     oled_class = class_create(OLED_CLASS_NAME);
//     if (IS_ERR(oled_class)) {
//         pr_err("Failed to create device class\n");
//         goto del_cdev;
//     }

//     // 4. /dev/oled_dev ���� ����
//     oled_device = device_create(oled_class, NULL, oled_dev_num, NULL, OLED_DEVICE_NAME);
//     if (IS_ERR(oled_device)) {
//         pr_err("Failed to create device file\n");
//         goto destroy_class;
//     }

//     // 5. I2C ����� �� Ŭ���̾�Ʈ ����
//     adapter = i2c_get_adapter(1); // I2C ���� 1��
//     if (!adapter) {
//         pr_err("Failed to get I2C adapter\n");
//         goto destroy_device;
//     }

//     oled_client = i2c_new_client_device(adapter, &board_info);
//     i2c_put_adapter(adapter);
//     if (!oled_client) {
//         pr_err("Failed to create I2C client\n");
//         goto destroy_device;
//     }

//     // 6. OLED ���÷��� �ʱ�ȭ
//     oled_display_init();
//     // oled_putc('H');
//     // oled_putc('i');
//     // oled_putc('!');

//     pr_info("OLED driver initialized successfully\n");
//     return 0;

// // --- ���� �߻� �� ���� ---
// destroy_device:
//     device_destroy(oled_class, oled_dev_num);
// destroy_class:
//     class_destroy(oled_class);
// del_cdev:
//     cdev_del(&oled_cdev);
// unreg_chrdev:
//     unregister_chrdev_region(oled_dev_num, 1);
//     return -1;
// }

// static void __exit oled_driver_exit(void) {
//     oled_clear();
//     oled_send_cmd(OLED_CMD_SET_DISPLAY_OFF);

//     i2c_unregister_device(oled_client);
//     device_destroy(oled_class, oled_dev_num);
//     class_destroy(oled_class);
//     cdev_del(&oled_cdev);
//     unregister_chrdev_region(oled_dev_num, 1);
//     pr_info("OLED driver exited\n");
// }

// module_init(oled_driver_init);
// module_exit(oled_driver_exit);

// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("Your Name");
// MODULE_DESCRIPTION("A simple I2C OLED character device driver for Raspberry Pi");
