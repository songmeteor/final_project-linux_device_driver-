#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/slab.h> // for kmalloc/kfree
#include <linux/delay.h> // for msleep

// --- OLED �� ����̽� ���� ---
#define OLED_DEVICE_NAME "oled_dev"
#define OLED_CLASS_NAME  "oled_class"
#define OLED_I2C_ADDR    0x3C // i2cdetect -y 1 �� Ȯ�ε� �ּ�

// --- OLED ���� ��� (SSD1306 ����) ---
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)

#define OLED_CMD_SET_DISPLAY_OFF 0xAE
#define OLED_CMD_SET_DISPLAY_ON  0xAF
#define OLED_CMD_SET_CONTRAST    0x81
#define OLED_CMD_ENTIRE_DISPLAY_ON 0xA4
#define OLED_CMD_NORMAL_DISPLAY 0xA6
#define OLED_CMD_SET_MEM_ADDR_MODE 0x20
#define OLED_CMD_SET_COL_ADDR    0x21
#define OLED_CMD_SET_PAGE_ADDR   0x22

// --- ���� ���� ---
static struct i2c_client *oled_client;
static struct cdev oled_cdev;
static dev_t oled_dev_num;
static struct class *oled_class;
static struct device *oled_device;

// --- ���� Ŀ�� ��ġ ---
static int cursor_x = 0;
static int cursor_y = 0;

// --- 5x7 ��Ʈ ������ (ASCII 32-126) ---
// (�������� ���� ��Ʈ �����ʹ� ����. ���� ���� �� ���⿡ ��Ʈ �迭�� �߰��ؾ� �մϴ�.)
// ����: const unsigned char font5x7[] = { ... };
// �� ���������� ��Ʈ ���� ���� ���� ���� �״�� ����ϴ� ������� �ܼ�ȭ�մϴ�.

// --- I2C ��� �Լ� ---
static int oled_send_cmd(u8 cmd) {
    return i2c_smbus_write_byte_data(oled_client, 0x00, cmd);
}

static int oled_send_data(u8 data) {
    return i2c_smbus_write_byte_data(oled_client, 0x40, data);
}

// --- OLED ȭ�� ���� �Լ� ---
static void oled_set_cursor(int x, int y) {
    oled_send_cmd(0xB0 + y); // Page
    oled_send_cmd(0x00 + (x & 0x0F)); // Lower nibble of column
    oled_send_cmd(0x10 + (x >> 4)); // Upper nibble of column
}

static void oled_clear(void) {
    int i;
    oled_set_cursor(0, 0);
    for (i = 0; i < OLED_WIDTH * OLED_PAGES; i++) {
        oled_send_data(0x00);
    }
    oled_set_cursor(0, 0);
    cursor_x = 0;
    cursor_y = 0;
}

static void oled_display_init(void) {
    msleep(100);
    oled_send_cmd(OLED_CMD_SET_DISPLAY_OFF);
    oled_send_cmd(0xD5); // Set Display Clock Divide Ratio
    oled_send_cmd(0x80);
    oled_send_cmd(0xA8); // Set Multiplex Ratio
    oled_send_cmd(OLED_HEIGHT - 1);
    oled_send_cmd(0xD3); // Set Display Offset
    oled_send_cmd(0x00);
    oled_send_cmd(0x40 | 0x0); // Set Start Line
    oled_send_cmd(0x8D); // Charge Pump Setting
    oled_send_cmd(0x14); // Enable Charge Pump
    oled_send_cmd(OLED_CMD_SET_MEM_ADDR_MODE);
    oled_send_cmd(0x00); // Horizontal Addressing Mode
    oled_send_cmd(0xA1); // Set Segment Re-map
    oled_send_cmd(0xC8); // Set COM Output Scan Direction
    oled_send_cmd(0xDA); // Set COM Pins Hardware Configuration
    oled_send_cmd(0x12);
    oled_send_cmd(OLED_CMD_SET_CONTRAST);
    oled_send_cmd(0xCF);
    oled_send_cmd(0xD9); // Set Pre-charge Period
    oled_send_cmd(0xF1);
    oled_send_cmd(0xDB); // Set VCOMH Deselect Level
    oled_send_cmd(0x40);
    oled_send_cmd(OLED_CMD_ENTIRE_DISPLAY_ON);
    oled_send_cmd(OLED_CMD_NORMAL_DISPLAY);
    oled_send_cmd(OLED_CMD_SET_DISPLAY_ON);
    oled_clear();
}

// --- ���� ���۷��̼� �Լ��� ---
static int oled_open(struct inode *inode, struct file *file) {
    pr_info("OLED driver: open() called\n");
    return 0;
}

static int oled_release(struct inode *inode, struct file *file) {
    pr_info("OLED driver: release() called\n");
    return 0;
}

// write �ý��� ���� ȣ��� �� ����Ǵ� �Լ�
static ssize_t oled_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    char *k_buf;
    int i;

    k_buf = kmalloc(len, GFP_KERNEL);
    if (!k_buf) {
        return -ENOMEM;
    }

    if (copy_from_user(k_buf, buf, len)) {
        kfree(k_buf);
        return -EFAULT;
    }

    // ���� ���ڿ��� OLED�� ���
    for (i = 0; i < len; i++) {
        if (k_buf[i] == '\n') { // �ٹٲ� ���� ó��
            cursor_y++;
            cursor_x = 0;
            if (cursor_y >= OLED_PAGES) {
                cursor_y = 0; // ȭ�� ���� �����ϸ� ó������
            }
            oled_set_cursor(cursor_x, cursor_y);
        } else if (k_buf[i] == '\f') { // �� �ǵ� ���ڷ� ȭ�� �����
            oled_clear();
        } else {
            // �� ���������� ��Ʈ ���� ���� ���� �״�� �����ͷ� ����
            // �����δ� ��Ʈ �����͸� ã�� ����ؾ� ��
            oled_send_data(k_buf[i]);
            cursor_x++;
            if (cursor_x >= OLED_WIDTH) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= OLED_PAGES) {
                    cursor_y = 0;
                }
                oled_set_cursor(cursor_x, cursor_y);
            }
        }
    }

    kfree(k_buf);
    return len;
}

static const struct file_operations oled_fops = {
    .owner   = THIS_MODULE,
    .open    = oled_open,
    .release = oled_release,
    .write   = oled_write,
};

// --- ��� �ʱ�ȭ �� ���� �Լ� ---
static int __init oled_driver_init(void) {
    struct i2c_adapter *adapter;
    struct i2c_board_info board_info = {
        .type = "ssd1306",
        .addr = OLED_I2C_ADDR,
    };

    // 1. ĳ���� ����̽� ���
    if (alloc_chrdev_region(&oled_dev_num, 0, 1, OLED_DEVICE_NAME) < 0) {
        pr_err("Failed to allocate device number\n");
        return -1;
    }
    pr_info("Device number allocated: Major %d, Minor %d\n", MAJOR(oled_dev_num), MINOR(oled_dev_num));

    // 2. cdev ����ü �ʱ�ȭ �� ���
    cdev_init(&oled_cdev, &oled_fops);
    if (cdev_add(&oled_cdev, oled_dev_num, 1) < 0) {
        pr_err("Failed to add cdev\n");
        goto unreg_chrdev;
    }

    // 3. ����̽� Ŭ���� ����
    oled_class = class_create(OLED_CLASS_NAME);
    if (IS_ERR(oled_class)) {
        pr_err("Failed to create device class\n");
        goto del_cdev;
    }

    // 4. /dev/oled_dev ���� ����
    oled_device = device_create(oled_class, NULL, oled_dev_num, NULL, OLED_DEVICE_NAME);
    if (IS_ERR(oled_device)) {
        pr_err("Failed to create device file\n");
        goto destroy_class;
    }

    // 5. I2C ����� �� Ŭ���̾�Ʈ ����
    adapter = i2c_get_adapter(1); // I2C ���� 1��
    if (!adapter) {
        pr_err("Failed to get I2C adapter\n");
        goto destroy_device;
    }

    oled_client = i2c_new_client_device(adapter, &board_info);
    i2c_put_adapter(adapter);
    if (!oled_client) {
        pr_err("Failed to create I2C client\n");
        goto destroy_device;
    }

    // 6. OLED ���÷��� �ʱ�ȭ
    oled_display_init();
    oled_send_data('H'); // ���� �޽��� (�ܼ�ȭ�� ���)
    oled_send_data('i');

    pr_info("OLED driver initialized successfully\n");
    return 0;

// --- ���� �߻� �� ���� ---
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

static void __exit oled_driver_exit(void) {
    oled_clear();
    oled_send_cmd(OLED_CMD_SET_DISPLAY_OFF);

    i2c_unregister_device(oled_client);
    device_destroy(oled_class, oled_dev_num);
    class_destroy(oled_class);
    cdev_del(&oled_cdev);
    unregister_chrdev_region(oled_dev_num, 1);
    pr_info("OLED driver exited\n");
}

module_init(oled_driver_init);
module_exit(oled_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple I2C OLED character device driver for Raspberry Pi");