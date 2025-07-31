#ifndef VS1003_COMMON_H
#define VS1003_COMMON_H

#include "vs1003.h"

#include <linux/list.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

#define VS1003_NAME "vs1003"

#define VS1003_KHZ 12288
#define VS1003_USEC(x) ( (((x)*1000) / VS1003_KHZ) + 1)
#define VS1003_MSEC(x) ( ((x) / VS1003_KHZ) + 1)

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

#define vs1003_printk(level, fmt, arg...) \
        printk(level "%s: " fmt "\n", __func__, ## arg)

#define vs1003_err(fmt, arg...) \
        vs1003_printk(KERN_ERR, fmt, ## arg)

#define vs1003_wrn(fmt, arg...) \
        vs1003_printk(KERN_WARNING, fmt, ## arg)

#define vs1003_inf(fmt, arg...) \
        vs1003_printk(KERN_INFO, fmt, ## arg)

#define vs1003_dbg(fmt, arg...)                     \
	do {                                            \
		if (*vs1003_debug > 0)                      \
			vs1003_printk(KERN_DEBUG, fmt, ## arg); \
        } while (0)

#define vs1003_nsy(fmt, arg...)                     \
	do {                                            \
		if (*vs1003_debug > 1)                      \
			vs1003_printk(KERN_DEBUG, fmt, ## arg); \
        } while (0)

extern int *vs1003_debug;

#endif