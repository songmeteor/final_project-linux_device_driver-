#ifndef __VS10XX_QUEUE_H__
#define __VS10XX_QUEUE_H__

#include <linux/slab.h>

#define VS10XX_QUEUE_MAX_ELEMENTS 2048
#define VS10XX_QUEUE_DATA_SIZE 32

typedef struct vs10xx_qel {
    struct list_head list;
    char data[VS10XX_QUEUE_DATA_SIZE];
} vs10xx_qel_t;

typedef struct vs10xx_queue {
    struct list_head head;
    int num_elements;
    spinlock_t lock;
} vs10xx_queue_t;

int vs10xx_queue_init(vs10xx_queue_t *q);
void vs10xx_queue_free(vs10xx_queue_t *q);
vs10xx_qel_t* vs10xx_queue_get_head(vs10xx_queue_t *q);
void vs10xx_queue_put_tail(vs10xx_queue_t *q, vs10xx_qel_t *el);

#endif /* __VS10XX_QUEUE_H__ */