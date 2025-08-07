#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "rotary_encoder"

#define GPIO_S1         512+17
#define GPIO_S2         512+27
#define GPIO_KEY        512+22

#define DEBOUNCE_TIME_MS 20

static int counter = 0;
static volatile int key_press_event = 0;
static int prev_s1 = 0;
static int prev_s2 = 0;

static struct timer_list debounce_timer;

static int irq_s1, irq_s2, irq_key;

static void debounce_timer_callback(struct timer_list *t);
static irqreturn_t irq_handler(int irq, void *dev_id);
static irqreturn_t key_handler(int irq, void *dev_id);

static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *filp, char __user *buf, size_t len, loff_t *off);

static dev_t dev_num;
static struct cdev my_cdev;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = my_open,
    .read = my_read,
	.release = my_release
};

static ssize_t my_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    char msg[16];
    int msg_len;
    int event_to_send;

    event_to_send = key_press_event;
    key_press_event = 0;

    // 현재 카운트 값을 문자열로 변환
    msg_len = sprintf(msg, "%d,%d\n", counter, event_to_send);

    // 커널 공간의 데이터를 유저 공간으로 복사
    if (copy_to_user(buf, msg, msg_len)) {
        pr_err("Rotary: Failed to copy data to user\n");
        return -EFAULT;
    }

    *off = msg_len;
    return msg_len;
}

static int my_open(struct inode *inode, struct file *file)
{
	int result = 0;

	pr_info("rotary_encoder driver opened.\n");

	// gpio 초기화
	gpio_request_one(GPIO_S1, GPIOF_IN, "rotary-s1");
    gpio_request_one(GPIO_S2, GPIOF_IN, "rotary-s2");
    gpio_request_one(GPIO_KEY, GPIOF_IN, "rotary-key");

	prev_s1 = gpio_get_value(GPIO_S1);
	prev_s2 = gpio_get_value(GPIO_S2);

	irq_s1 = gpio_to_irq(GPIO_S1);
	irq_s2 = gpio_to_irq(GPIO_S2);
	irq_key = gpio_to_irq(GPIO_KEY);

	timer_setup(&debounce_timer, debounce_timer_callback, 0); // debounce_timer가 20ms가 되면 콜백함수 실행

	result = request_threaded_irq(irq_s1, NULL, irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "rotary-s1-irq", NULL);
    result |= request_threaded_irq(irq_s2, NULL, irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "rotary-s2-irq", NULL);
    result |= request_threaded_irq(irq_key, NULL, key_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "rotary-key-irq", NULL);

	if(result)
	{
		pr_err("rotary_encoder driver : failed to request IRQ\n");
		return result;
	}

	pr_info("rotary_encoder driver : opened successfully.\n");

	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("rotary_encoder driver : device closed\n");

	del_timer_sync(&debounce_timer);
	free_irq(irq_s1, NULL);
	free_irq(irq_s2, NULL);
	free_irq(irq_key, NULL);

	gpio_free(GPIO_S1);
	gpio_free(GPIO_S2);
	gpio_free(GPIO_KEY);

	return 0;
}

static void debounce_timer_callback(struct timer_list *t)
{
   if(!gpio_get_value(GPIO_KEY)) key_press_event = 1;
}

static irqreturn_t key_handler(int irq, void *dev_id)
{
    // debounce_timer 구조체가 현재 시각으로 부터 20ms를 센다.
    mod_timer(&debounce_timer, jiffies + msecs_to_jiffies(DEBOUNCE_TIME_MS));	
    return IRQ_HANDLED;
}


static irqreturn_t irq_handler(int irq, void *dev_id)
{
	int s1 = gpio_get_value(GPIO_S1);
    int s2 = gpio_get_value(GPIO_S2);

     if(s1 != prev_s1 || s2 != prev_s2)
     {
             int prev_state = (prev_s1 << 1) | prev_s2;
             int curr_state = (s1 << 1) | s2;

             //시계 방향 (00, 10, 11, 01, 00)
             if(prev_state == 0b01 && curr_state == 0b00)
             {
                  if(counter < 0b1111) counter++;
             }
             // 반시계 방향 (00, 01, 11, 10, 00)
             else if(prev_state == 0b10 && curr_state == 0b00)
             {
                  if(counter > 0) counter--;
             }
         prev_s1 = s1;
         prev_s2 = s2;
     }	
	return IRQ_HANDLED;
}

static int __init rotary_encoder_init(void)
{
	pr_info("rotary_encoder driver : Initializing C dev...\n");

	// 1. 주/부 번호 할당
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        pr_err("Failed to allocate major number\n");
        return -1;
    }
    pr_info("Major number allocated: %d\n", MAJOR(dev_num));

    // 2. 캐릭터 디바이스 초기화 및 등록
    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        pr_err("Failed to add the cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    pr_info("rotary_encoder driver : Ready. Create device file with 'mknod'.\n");
    return 0;
}

static void __exit rotary_encoder_exit(void)
{
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("rotary_encoder driver : Character device unloaded.\n");
}

module_init(rotary_encoder_init);
module_exit(rotary_encoder_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SYS");
MODULE_DESCRIPTION("Rotary Encoder Driver (Manual mknod)");




