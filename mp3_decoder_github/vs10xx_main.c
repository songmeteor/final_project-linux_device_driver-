#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include "vs10xx.h"
#include "vs10xx_queue.h"
#include "vs10xx_iocomm.h"
#include "vs10xx_device.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rajiv Biswas");
MODULE_DESCRIPTION("VS1003/VS1053 Audio Codec Driver");

#define DRIVER_NAME "vs10xx"
#define VS10XX_IOCTL_BASE 'v' // ���������� ���� ����̹��� ���ÿ� �۵��ϴµ� ���� ����� ������ �� ����� � ����̹��� ������� �𸣱⶧���� �浹 ������ ���� ������ ��ȣ�� ���Ѵ�.
#define VS10XX_SET_VOL _IOW(VS10XX_IOCTL_BASE, 1, unsigned int) // ���� ���� ��ɾ� ����, ([������ ��ȣ], [0:����, 1:��������, 2:������ �׽�Ʈ], [user_space���� Ŀ�η� ������ ������ Ÿ��])
                                                                // ���� ���α׷��� ioctl(fd, VS10XX_SET_VOL, &volume_data)�� ȣ���ϸ� Ŀ���� VS10XX_SET_VOL�� ���� v ��ȣ�� ���� vs10xx����̹��� ���� �ų�! ��� �� �� ����

static dev_t vs10xx_dev_t;
struct class *vs10xx_class;
struct vs10xx_chip vs10xx_chips[VS10XX_MAX_DEVICES]; //vs10xx_chip�� vs10xx.h�� ���ǵǾ��ִ� Ĩ�� �����ϴµ� �ʿ��� ��� ������ �ִ� ����ü

static int vs10xx_open(struct inode *inode, struct file *filp) {
    struct vs10xx_chip *chip = container_of(inode->i_cdev, struct vs10xx_chip, cdev);
    filp->private_data = chip;
    PDEBUG("vs10xx_open\n");
    return 0;
}

static int vs10xx_release(struct inode *inode, struct file *filp) {
    PDEBUG("vs10xx_release\n");
    return 0;
}

static ssize_t vs10xx_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct vs10xx_chip *chip = filp->private_data;
    size_t total_written = 0;
    vs10xx_qel_t *qel;   // 32byte ���ۿ� �ٸ� 32byte�� �յ� �����Ϳ� ������ �� �ִ� ���� ���ǵ� vs10xx_qel_t 

    // ����ڰ� ��û�� �����͸� 32����Ʈ ûũ�� ������ ó��
    while (total_written < count) {
        // vs10xx_qel_t *qel;
        size_t chunk_size = min((size_t)VS10XX_QUEUE_DATA_SIZE, count - total_written);

        // 1. ���� Ǯ���� �� ���۸� ������ (������ ���)
        if (wait_event_interruptible(chip->tx_wq, (qel = vs10xx_queue_get_head(&chip->tx_pool_q)) != NULL)) {  // tx_pool_q �� �� ���۰� ������(32byte ������ 2048�� ������ ��� tx_data_q �� ������ϋ�) tx_wp�� ���
            return -ERESTARTSYS;
        }

        // 2. ����� �������� �����͸� Ŀ�� ���۷� ����
        if (copy_from_user(qel->data, buf + total_written, chunk_size)) { // write �ý��� ���� ���� ������ MP3 ������ ���(buf)����, �̹��� ������ �κ��� ���� �ּҿ� �ִ� ���� chunk_size ��ŭ qel->data�� ����
            vs10xx_queue_put_tail(&chip->tx_pool_q, qel); // ���� �� ���� �ݳ�
            return -EFAULT;
        }

        // 3. �����Ͱ� ä���� ���۸� ������ ť�� ����
        vs10xx_queue_put_tail(&chip->tx_data_q, qel);
        total_written += chunk_size;
    }

    // 4. ������ ť�� �ִ� ��� �����͸� �ϵ����� ���� �õ�
    while ((qel = vs10xx_queue_get_head(&chip->tx_data_q)) != NULL) {
        // Ĩ�� �����͸� ���� �غ� �� ������ ���
        if (!vs10xx_io_wtready(chip->id, 100)) {
            // �غ���� �ʾ�����, ������ �����͸� �ٽ� ť�� �� �տ� �ְ� ���
            list_add(&qel->list, &chip->tx_data_q.head);
            chip->tx_data_q.num_elements++;
            break; 
        }

        // ������ ����
        if (vs10xx_io_data_tx(chip->id, qel->data, VS10XX_QUEUE_DATA_SIZE) < 0) {
            pr_err("vs10xx: Failed to send data via SPI\n");
            // ���� ���� �ÿ��� ���۴� Ǯ�� �ݳ�
        }
        
        // ����� ���۴� �ٽ� ���� Ǯ�� �ݳ�
        vs10xx_queue_put_tail(&chip->tx_pool_q, qel);

        // ���۰� ����ٰ� ��ٸ��� �ٸ� ���μ����� ����
        wake_up_interruptible(&chip->tx_wq);
    }

    return total_written;
}

static long vs10xx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct vs10xx_chip *chip = filp->private_data;
    int ret = 0;
    unsigned int vol;
    unsigned char left, right;

    if (_IOC_TYPE(cmd) != VS10XX_IOCTL_BASE) return -ENOTTY;
    
    switch (cmd) {
        case VS10XX_SET_VOL:
            if (copy_from_user(&vol, (void __user *)arg, sizeof(vol))) return -EFAULT;
            left = (vol >> 8) & 0xFF;
            right = vol & 0xFF;
            vs10xx_device_w_sci_reg(chip->id, 0x0B, left, right);
            break;
        default:
            return -ENOTTY;
    }
    return ret;
}

static const struct file_operations vs10xx_fops = {
    .owner = THIS_MODULE,
    .open = vs10xx_open,
    .release = vs10xx_release,
    .write = vs10xx_write,
    .unlocked_ioctl = vs10xx_ioctl,
};

static int vs10xx_spi_ctrl_probe(struct spi_device *spi) {
    int device_id;
    struct device *dev = &spi->dev;

    of_property_read_u32(dev->of_node, "device_id", &device_id);
    if(device_id >= VS10XX_MAX_DEVICES) return -EINVAL;

    vs10xx_chips[device_id].id = device_id;
    vs10xx_chips[device_id].spi_ctrl = spi;

    /* Device Tree�� 'reset-gpios'�� 'reset'�̶�� �̸����� ��û */
    vs10xx_chips[device_id].gpio_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(vs10xx_chips[device_id].gpio_reset)) {
        dev_err(dev, "Failed to get reset gpio\n");
        return PTR_ERR(vs10xx_chips[device_id].gpio_reset);
    }

    /* Device Tree�� 'dreq-gpios'�� 'dreq'�̶�� �̸����� ��û */
    vs10xx_chips[device_id].gpio_dreq = devm_gpiod_get(dev, "dreq", GPIOD_IN);
    if (IS_ERR(vs10xx_chips[device_id].gpio_dreq)) {
        dev_err(dev, "Failed to get dreq gpio\n");
        return PTR_ERR(vs10xx_chips[device_id].gpio_dreq);
    }
    
    spi_set_drvdata(spi, &vs10xx_chips[device_id]);

    dev_info(dev, "Control probe for device %d successful\n", device_id);
    
    return 0;
}

static int vs10xx_spi_data_probe(struct spi_device *spi) {
    int device_id;
    of_property_read_u32(spi->dev.of_node, "device_id", &device_id);
    if(device_id >= VS10XX_MAX_DEVICES) return -EINVAL;
        
    vs10xx_chips[device_id].spi_data = spi;
    spi_set_drvdata(spi, &vs10xx_chips[device_id]);
    
    printk(KERN_INFO "vs10xx: Data probe for device %d\n", device_id);

    // After both probes are done, initialize the device
    vs10xx_io_init(device_id);
    vs10xx_device_init(device_id);
    
    return 0;
}

static const struct of_device_id vs10xx_ctrl_id[] = {
    { .compatible = "vs1003-ctrl" },
    {}
};
MODULE_DEVICE_TABLE(of, vs10xx_ctrl_id);

static struct spi_driver vs10xx_spi_ctrl = {
    .driver = { .name = "vs10xx-ctrl", .of_match_table = of_match_ptr(vs10xx_ctrl_id) },
    .probe = vs10xx_spi_ctrl_probe,
};

static const struct of_device_id vs10xx_data_id[] = {
    { .compatible = "vs1003-data" },
    {}
};
MODULE_DEVICE_TABLE(of, vs10xx_data_id);

static struct spi_driver vs10xx_spi_data = {
    .driver = { .name = "vs10xx-data", .of_match_table = of_match_ptr(vs10xx_data_id) },
    .probe = vs10xx_spi_data_probe,
};

static int __init vs10xx_init(void) {
    int ret;
    int i;

    ret = alloc_chrdev_region(&vs10xx_dev_t, 0, VS10XX_MAX_DEVICES, DRIVER_NAME);
    if (ret) {
        PERR("Failed to allocate char device region\n");
        return ret;
    }

    vs10xx_class = class_create(DRIVER_NAME);
    if (IS_ERR(vs10xx_class)) {
        unregister_chrdev_region(vs10xx_dev_t, VS10XX_MAX_DEVICES);
        return PTR_ERR(vs10xx_class);
    }
    
    for (i = 0; i < VS10XX_MAX_DEVICES; i++) {
        cdev_init(&vs10xx_chips[i].cdev, &vs10xx_fops);
        vs10xx_chips[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&vs10xx_chips[i].cdev, MKDEV(MAJOR(vs10xx_dev_t), i), 1);
        if (ret) {
            PERR("cdev_add failed for device %d\n", i);
            continue;
        }

        vs10xx_chips[i].dev = device_create(vs10xx_class, NULL, MKDEV(MAJOR(vs10xx_dev_t), i), NULL, "%s-%d", DRIVER_NAME, i);
        if(IS_ERR(vs10xx_chips[i].dev)) {
             PERR("device_create failed for device %d\n", i);
        }

        vs10xx_queue_init(&vs10xx_chips[i].tx_pool_q);
        INIT_LIST_HEAD(&vs10xx_chips[i].tx_data_q.head);
        vs10xx_chips[i].tx_data_q.num_elements = 0;
        spin_lock_init(&vs10xx_chips[i].tx_data_q.lock);
        
        init_waitqueue_head(&vs10xx_chips[i].tx_wq);
    }

    spi_register_driver(&vs10xx_spi_ctrl);
    spi_register_driver(&vs10xx_spi_data);
    
    printk(KERN_INFO "vs10xx: Driver loaded\n");
    return 0;
}

static void __exit vs10xx_exit(void) {
    int i;
    spi_unregister_driver(&vs10xx_spi_ctrl);
    spi_unregister_driver(&vs10xx_spi_data);
    
    for (i = 0; i < VS10XX_MAX_DEVICES; i++) {
        device_destroy(vs10xx_class, MKDEV(MAJOR(vs10xx_dev_t), i));
        cdev_del(&vs10xx_chips[i].cdev);
        vs10xx_queue_free(&vs10xx_chips[i].tx_pool_q);
        vs10xx_io_exit(i);
    }
    
    class_destroy(vs10xx_class);
    unregister_chrdev_region(vs10xx_dev_t, VS10XX_MAX_DEVICES);
    printk(KERN_INFO "vs10xx: Driver unloaded\n");
}

module_init(vs10xx_init);
module_exit(vs10xx_exit);