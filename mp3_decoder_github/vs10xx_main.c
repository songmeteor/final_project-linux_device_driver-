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
#define VS10XX_IOCTL_BASE 'v' // 리눅스에는 여러 드라이버가 동시에 작동하는데 같은 명령을 보내면 이 명령이 어떤 드라이버의 명령인지 모르기때문에 충돌 방지를 위해 고유한 암호를 정한다.
#define VS10XX_SET_VOL _IOW(VS10XX_IOCTL_BASE, 1, unsigned int) // 볼륨 설정 명령어 정의, ([고유한 암호], [0:리셋, 1:볼륨설정, 2:사인파 테스트], [user_space에서 커널로 전달할 데이터 타입])
                                                                // 유저 프로그램이 ioctl(fd, VS10XX_SET_VOL, &volume_data)를 호출하면 커널은 VS10XX_SET_VOL을 보고 v 암호를 쓰는 vs10xx드라이버를 위한 거네! 라고 알 수 있음

static dev_t vs10xx_dev_t;
struct class *vs10xx_class;
struct vs10xx_chip vs10xx_chips[VS10XX_MAX_DEVICES]; //vs10xx_chip는 vs10xx.h에 정의되어있는 칩을 제어하는데 필요한 모든 정보가 있는 구조체

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
    vs10xx_qel_t *qel;   // 32byte 버퍼와 다른 32byte의 앞뒤 데이터와 연결할 수 있는 고리가 정의된 vs10xx_qel_t 

    // 사용자가 요청한 데이터를 32바이트 청크로 나누어 처리
    while (total_written < count) {
        // vs10xx_qel_t *qel;
        size_t chunk_size = min((size_t)VS10XX_QUEUE_DATA_SIZE, count - total_written);

        // 1. 버퍼 풀에서 빈 버퍼를 가져옴 (없으면 대기)
        if (wait_event_interruptible(chip->tx_wq, (qel = vs10xx_queue_get_head(&chip->tx_pool_q)) != NULL)) {  // tx_pool_q 에 빈 버퍼가 없을떄(32byte 데이터 2048개 샘플이 모두 tx_data_q 에 대기중일떄) tx_wp에 대기
            return -ERESTARTSYS;
        }

        // 2. 사용자 공간에서 데이터를 커널 버퍼로 복사
        if (copy_from_user(qel->data, buf + total_written, chunk_size)) { // write 시스템 콜을 통해 전달한 MP3 데이터 덩어리(buf)에서, 이번에 복사할 부분의 시작 주소에 있는 값을 chunk_size 만큼 qel->data로 복사
            vs10xx_queue_put_tail(&chip->tx_pool_q, qel); // 실패 시 버퍼 반납
            return -EFAULT;
        }

        // 3. 데이터가 채워진 버퍼를 데이터 큐에 넣음
        vs10xx_queue_put_tail(&chip->tx_data_q, qel);
        total_written += chunk_size;
    }

    // 4. 데이터 큐에 있는 모든 데이터를 하드웨어로 전송 시도
    while ((qel = vs10xx_queue_get_head(&chip->tx_data_q)) != NULL) {
        // 칩이 데이터를 받을 준비가 될 때까지 대기
        if (!vs10xx_io_wtready(chip->id, 100)) {
            // 준비되지 않았으면, 꺼내온 데이터를 다시 큐의 맨 앞에 넣고 대기
            list_add(&qel->list, &chip->tx_data_q.head);
            chip->tx_data_q.num_elements++;
            break; 
        }

        // 데이터 전송
        if (vs10xx_io_data_tx(chip->id, qel->data, VS10XX_QUEUE_DATA_SIZE) < 0) {
            pr_err("vs10xx: Failed to send data via SPI\n");
            // 전송 실패 시에도 버퍼는 풀로 반납
        }
        
        // 사용한 버퍼는 다시 버퍼 풀로 반납
        vs10xx_queue_put_tail(&chip->tx_pool_q, qel);

        // 버퍼가 비었다고 기다리는 다른 프로세스를 깨움
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

    /* Device Tree의 'reset-gpios'를 'reset'이라는 이름으로 요청 */
    vs10xx_chips[device_id].gpio_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(vs10xx_chips[device_id].gpio_reset)) {
        dev_err(dev, "Failed to get reset gpio\n");
        return PTR_ERR(vs10xx_chips[device_id].gpio_reset);
    }

    /* Device Tree의 'dreq-gpios'를 'dreq'이라는 이름으로 요청 */
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