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
    struct spi_device *spi_ctrl;  // ����� spi ��ſ� �ʿ��� ������ ��� ��ü�� ������
    struct spi_device *spi_data;  // �����Ϳ� spi ��ſ� �ʿ��� ����
    
    struct gpio_desc *gpio_reset;  // ���� GPIO �� �����
    struct gpio_desc *gpio_dreq;   // DREQ GPIO �� �����

    struct cdev cdev;   //ĳ���� ����̽� ������ ����� ���� ����
    struct device *dev; 
    
    struct spi_message msg; // transfer[0], transfer[1]�� ���� �������� msg��� �ù���ڿ� ��Ƽ� �� ���ڸ� spi_sync()��� �Լ��� �����ϸ� Ŀ���� ����̹��� �ڵ����� SPI ������ش�.
    struct spi_transfer transfer[2]; //spi_transfer �� ������ Ŀ���� ������ ����ü, �ѹ��� ���������� �ְ� ���� ������ ���. ex) transfer[0]���� tx_buf[4]�� ������, transfer[1]���� rx_buf[2]�� ���� �޾ƿͶ�
    u8 tx_buf[4];   // 4����Ʈ�� ����, ([����ڵ�, �ּ�, �����ͻ���, ����������]) �� ������ vs10xx Ĩ�� ���� ��ɾ�
    u8 rx_buf[2];   // Ĩ�� ������ �����͸� �����ϴ� ����

    wait_queue_head_t tx_wq; // wait_queue_head_t�� ������ Ŀ���� ����ȭ ���� �� �ϳ�, Ư�� ������ ��ٸ��� ���μ������� ��� ����δ� ����
                            // vs10xx_write �Լ��� MP3�����͸� ť�� ���������� ��, tx_pool_q �� ��������� �۾��� ������ �� �����ϱ� tx_wq�� ���.
    int tx_busy;

    vs10xx_queue_t tx_pool_q; // vs10xx_queue_t �� vs10xx_queue.h �� ���� ���� ť ����ü�̴�. �� ť�ȿ��� vs10xx_qel_t��� ���� ������ �����̵��µ� �� ������ 32����Ʈ ũ���� MP3�����͸� ���� �� �ִ�.
                              // ���� ���α׷��� write �Լ��� ���� MP3 �����͸� ������, ����̹��� �� tx_pool_q���� �� ���۸� �ϳ� ���� �װ��� �����͸� ä���.
    vs10xx_queue_t tx_data_q; // write �Լ��� �����͸� ä�� ���۸� �� tx_data_q�� �� �ڿ� ���� �����. �׷��� ����̹��� SPI ���� ��Ʈ�� �� ��⿭�� �� �տ������� ���۸� �ϳ��� ���� �ϵ����� ����
};                            // ���� ������ ���۸� �ٽ� tx_pool_q�� ���� ��Ȱ��

extern struct vs10xx_chip vs10xx_chips[VS10XX_MAX_DEVICES];

#endif /* __VS10XX_H__ */