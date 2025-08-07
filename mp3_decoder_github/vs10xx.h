#ifndef __VS10XX_H__
#define __VS10XX_H__

#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include "vs10xx_queue.h"
#include <linux/gpio/consumer.h>

#define VS10XX_MAX_DEVICES 2
#define VS10XX_MAX_TRANSFER_SIZE 32

/* Debugging */
#ifdef VS10XX_DEBUG
#  undef PDEBUG
#  define PDEBUG(fmt, args...) printk( KERN_DEBUG "vs10xx: " fmt, ## args)
#else
#  define PDEBUG(fmt, args...)
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...)

#undef PERR
#define PERR(fmt, args...) printk( KERN_ERR "vs10xx: " fmt, ## args)

/* Device structure */
struct vs10xx_chip {
    int id;
    struct spi_device *spi_ctrl;  // 제어용 spi 통신에 필요한 정보가 담긴 객체의 포인터
    struct spi_device *spi_data;  // 데이터용 spi 통신에 필요한 정보
    
    struct gpio_desc *gpio_reset;  // 리셋 GPIO 핀 제어권
    struct gpio_desc *gpio_dreq;   // DREQ GPIO 핀 제어권

    struct cdev cdev;   //캐릭터 디바이스 파일을 만들기 위한 정보
    struct device *dev; 
    
    struct spi_message msg; // transfer[0], transfer[1]과 같은 봉투들을 msg라는 택배상자에 담아서 이 상자를 spi_sync()라는 함수에 전달하면 커널의 드라이버가 자동으로 SPI 통신해준다.
    struct spi_transfer transfer[2]; //spi_transfer 는 리눅스 커널이 정의한 구조체, 한번에 연속적으로 주고 받을 데이터 덩어리. ex) transfer[0]에는 tx_buf[4]를 보내고, transfer[1]에는 rx_buf[2]로 답을 받아와라
    u8 tx_buf[4];   // 4바이트의 버퍼, ([명령코드, 주소, 데이터상위, 데이터하위]) 로 구성된 vs10xx 칩에 보낼 명령어
    u8 rx_buf[2];   // 칩이 응답한 데이터를 저장하는 버퍼

    wait_queue_head_t tx_wq; // wait_queue_head_t는 리눅스 커널의 동기화 도구 중 하나, 특정 조건을 기다리는 프로세스들을 잠시 재워두는 대기실
                            // vs10xx_write 함수가 MP3데이터를 큐에 넣으려고할 때, tx_pool_q 가 비어있으면 작업을 수행할 수 없으니까 tx_wq에 대기.
    int tx_busy;

    vs10xx_queue_t tx_pool_q; // vs10xx_queue_t 는 vs10xx_queue.h 에 직접 만든 큐 구조체이다. 이 큐안에는 vs10xx_qel_t라는 작은 데이터 조각이들어가는데 각 조각은 32바이트 크기의 MP3데이터를 담을 수 있다.
                              // 유저 프로그램이 write 함수를 통해 MP3 데이터를 보내면, 드라이버는 이 tx_pool_q에서 빈 버퍼를 하나 꺼내 그곳에 데이터를 채운다.
    vs10xx_queue_t tx_data_q; // write 함수가 데이터를 채운 버퍼를 이 tx_data_q의 맨 뒤에 줄을 세운다. 그러면 드라이버의 SPI 전송 파트는 이 대기열의 맨 앞에서부터 버퍼를 하나씩 꺼내 하드웨어로 전송
};                            // 이제 비어버린 버퍼를 다시 tx_pool_q로 보내 재활용

extern struct vs10xx_chip vs10xx_chips[VS10XX_MAX_DEVICES];

#endif /* __VS10XX_H__ */