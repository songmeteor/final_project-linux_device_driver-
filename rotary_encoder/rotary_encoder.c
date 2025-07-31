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

#define GPIO_S1         535
#define GPIO_S2         536
#define GPIO_KEY        537
#define LED_START_PIN   524 // LED는 12번 ~ 18번 핀에 연결

#define DEBOUNCE_TIME_MS 20

static int counter = 0;
static int direction = 0; //시계방향 1, 반시계 방향 2
static bool key_state = false;
static int prev_s1 = 0;
static int prev_s2 = 0;

static struct timer_list debounce_timer;

static int irq_s1, irq_s2, irq_key;

static void update_led(void);
static void debounce_timer_callback(struct timer_list *t);
static irqreturn_t irq_handler(int irq, void *dev_id);
static irqreturn_t key_handler(int irq, void *dev_id);

static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);

static dev_t dev_num;
static struct cdev my_cdev;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release
};

static int my_open(struct inode *inode, struct file *file)
{
	int result = 0;

	pr_info("rotary_encoder driver opened.\n");

	// gpio 초기화
	gpio_request_one(GPIO_S1, GPIOF_IN, "rotary-s1");
    gpio_request_one(GPIO_S2, GPIOF_IN, "rotary-s2");
    gpio_request_one(GPIO_KEY, GPIOF_IN, "rotary-key");
	for(int i = 0; i < 7; i++) 
	{
		gpio_request_one(LED_START_PIN + i, GPIOF_OUT_INIT_LOW, "led");
	}

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

    for(int i = 0; i < 7; i++)
    {
        gpio_free(LED_START_PIN + i);
    }

	return 0;
}


static void update_led(void)
{
	int i;
	unsigned int led_data = 0;  
	// 0~3 : count, 4 : key, 5~6 : direction

	led_data |= (counter & 0x0F);
	led_data |= (key_state & 0x01) << 4; 
	led_data |= (direction & 0x03) << 5;

	for(i = 0; i < 7; i++)
    {
		gpio_set_value(LED_START_PIN + i, (led_data >> i) & 0x01);
	}
}

static void debounce_timer_callback(struct timer_list *t)
{
   bool curr_key_down = !gpio_get_value(GPIO_KEY);

   if(curr_key_down) key_state = !key_state;
   
   update_led();	
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
                  direction = 1; // 시계방향
             }
             // 반시계 방향 (00, 01, 11, 10, 00)
             else if(prev_state == 0b10 && curr_state == 0b00)
             {
                  if(counter > 0) counter--;
                  direction = 2; // 반시계 방향
             }
         prev_s1 = s1;
         prev_s2 = s2;
     }
    update_led();	
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




