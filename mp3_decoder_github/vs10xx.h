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
    struct spi_device *spi_ctrl;
    struct spi_device *spi_data;
    
    struct gpio_desc *gpio_reset;
    struct gpio_desc *gpio_dreq;

    struct cdev cdev;
    struct device *dev;
    
    struct spi_message msg;
    struct spi_transfer transfer[2];
    u8 tx_buf[4];
    u8 rx_buf[2];

    wait_queue_head_t tx_wq;
    int tx_busy;

    vs10xx_queue_t tx_pool_q;
    vs10xx_queue_t tx_data_q;
};

extern struct vs10xx_chip vs10xx_chips[VS10XX_MAX_DEVICES];

#endif /* __VS10XX_H__ */