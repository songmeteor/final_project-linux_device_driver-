#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

// 캐릭터 디바이스 관련 헤더는 더 이상 필요 없습니다.
// #include <linux/fs.h>
// #include <linux/cdev.h>
// #include <linux/uaccess.h>

#define MODULE_NAME "rotary_encoder"

// 충돌 가능성이 낮은 핀 사용
#define GPIO_S1       13
#define GPIO_S2       19
#define GPIO_KEY      26
#define LED_START_PIN 12

#define DEBOUNCE_TIME_MS 20

// --- 전역 변수 ---
static int counter = 0;
static int direction = 0; // 0: none, 1: CW, 2: CCW
static bool key_state = false;
static int prev_s1 = 0;
static int prev_s2 = 0;
static struct timer_list debounce_timer;
static int irq_s1, irq_s2, irq_key;

// --- 함수 프로토타입 ---
static void update_led(void);
static void debounce_timer_callback(struct timer_list *t);
static irqreturn_t irq_handler(int irq, void *dev_id);

// LED 상태를 업데이트하는 함수
static void update_led(void)
{
    int i;
    unsigned int led_data = 0;
    // 0~3 : count, 4 : key, 5~6 : direction
    led_data |= (counter & 0x0F);
    led_data |= (key_state & 0x01) << 4;
    led_data |= (direction & 0x03) << 5;

    for (i = 0; i < 7; i++) {
        gpio_set_value(LED_START_PIN + i, (led_data >> i) & 0x01);
    }
}

// 디바운싱 타이머가 만료되면 호출될 콜백 함수
static void debounce_timer_callback(struct timer_list *t)
{
    int s1 = gpio_get_value(GPIO_S1);
    int s2 = gpio_get_value(GPIO_S2);
    // 버튼은 Active-low로 가정 (눌렀을 때 0)
    bool curr_key_down = !gpio_get_value(GPIO_KEY);

    if (curr_key_down) {
        key_state = !key_state; // 토글 방식
    }

    if (s1 != prev_s1 || s2 != prev_s2) {
        int prev_state = (prev_s1 << 1) | prev_s2;
        int curr_state = (s1 << 1) | s2;

        // 시계 방향 (00 -> 10 -> 11 -> 01 -> 00)
        if ((prev_state == 0b00 && curr_state == 0b10) ||
            (prev_state == 0b10 && curr_state == 0b11) ||
            (prev_state == 0b11 && curr_state == 0b01) ||
            (prev_state == 0b01 && curr_state == 0b00)) {
            if (counter < 0b1111) counter++;
            direction = 1; // 시계방향
        }
        // 반시계 방향 (00 -> 01 -> 11 -> 10 -> 00)
        else if ((prev_state == 0b00 && curr_state == 0b01) ||
                 (prev_state == 0b01 && curr_state == 0b11) ||
                 (prev_state == 0b11 && curr_state == 0b10) ||
                 (prev_state == 0b10 && curr_state == 0b00)) {
            if (counter > 0) counter--;
            direction = 2; // 반시계 방향
        }
        prev_s1 = s1;
        prev_s2 = s2;
    }
    update_led();
}

// 인터럽트가 발생했을 때 호출될 핸들러
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    // 20ms 뒤에 타이머 콜백이 실행되도록 타이머를 수정
    mod_timer(&debounce_timer, jiffies + msecs_to_jiffies(DEBOUNCE_TIME_MS));
    return IRQ_HANDLED;
}

// 모듈 초기화 함수 (insmod 시 호출)
static int __init rotary_encoder_init(void)
{
    int i;
    int result = 0;

    pr_info("%s: Loading driver...\n", MODULE_NAME);

    // 1. GPIO 핀 요청
    if (gpio_request_one(GPIO_S1, GPIOF_IN, "rotary-s1")) goto fail;
    if (gpio_request_one(GPIO_S2, GPIOF_IN, "rotary-s2")) goto free_gpio_s1;
    if (gpio_request_one(GPIO_KEY, GPIOF_IN, "rotary-key")) goto free_gpio_s2;
    
    for (i = 0; i < 7; i++) {
        if (gpio_request_one(LED_START_PIN + i, GPIOF_OUT_INIT_LOW, "led")) {
            while (--i >= 0) gpio_free(LED_START_PIN + i);
            goto free_gpio_key;
        }
    }

    // 2. 인터럽트 번호 가져오기
    irq_s1 = gpio_to_irq(GPIO_S1);
    irq_s2 = gpio_to_irq(GPIO_S2);
    irq_key = gpio_to_irq(GPIO_KEY);
    
    // 3. 타이머 설정
    timer_setup(&debounce_timer, debounce_timer_callback, 0);

    // 4. 인터럽트 핸들러 등록
    // IRQF_ONESHOT 플래그를 제거하여 매 인터럽트마다 핸들러가 disable되지 않도록 합니다.
    result = request_threaded_irq(irq_s1, NULL, irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "rotary-s1-irq", NULL);
    if (result) goto free_leds;
    result = request_threaded_irq(irq_s2, NULL, irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "rotary-s2-irq", NULL);
    if (result) goto free_irq_s1;
    result = request_threaded_irq(irq_key, NULL, irq_handler, IRQF_TRIGGER_FALLING, "rotary-key-irq", NULL);
    if (result) goto free_irq_s2;

    // 초기 상태 저장
    prev_s1 = gpio_get_value(GPIO_S1);
    prev_s2 = gpio_get_value(GPIO_S2);

    pr_info("%s: Driver loaded successfully.\n", MODULE_NAME);
    return 0;

// --- 에러 발생 시 자원 해제 경로 ---
free_irq_s2:
    free_irq(irq_s2, NULL);
free_irq_s1:
    free_irq(irq_s1, NULL);
free_leds:
    for (i = 0; i < 7; i++) gpio_free(LED_START_PIN + i);
free_gpio_key:
    gpio_free(GPIO_KEY);
free_gpio_s2:
    gpio_free(GPIO_S2);
free_gpio_s1:
    gpio_free(GPIO_S1);
fail:
    pr_err("%s: Failed to load driver.\n", MODULE_NAME);
    return -1;
}

// 모듈 종료 함수 (rmmod 시 호출)
static void __exit rotary_encoder_exit(void)
{
    int i;
    pr_info("%s: Unloading driver...\n", MODULE_NAME);
    
    // 등록된 자원들을 역순으로 해제
    del_timer_sync(&debounce_timer);
    free_irq(irq_s1, NULL);
    free_irq(irq_s2, NULL);
    free_irq(irq_key, NULL);
    
    gpio_free(GPIO_S1);
    gpio_free(GPIO_S2);
    gpio_free(GPIO_KEY);
    
    for (i = 0; i < 7; i++) {
        gpio_free(LED_START_PIN + i);
    }
    
    pr_info("%s: Driver unloaded.\n", MODULE_NAME);
}

module_init(rotary_encoder_init);
module_exit(rotary_encoder_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SYS");
MODULE_DESCRIPTION("Rotary Encoder Driver (No System Call)");
