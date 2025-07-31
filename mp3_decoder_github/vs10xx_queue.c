#include <linux/list.h>
#include "vs10xx_queue.h"

int vs10xx_queue_init(vs10xx_queue_t *q) {
    int i;
    INIT_LIST_HEAD(&q->head);
    q->num_elements = 0;
    spin_lock_init(&q->lock);

    for (i = 0; i < VS10XX_QUEUE_MAX_ELEMENTS; i++) {
        vs10xx_qel_t *new_el = kmalloc(sizeof(vs10xx_qel_t), GFP_KERNEL);
        if (!new_el) {
            vs10xx_queue_free(q);
            return -ENOMEM;
        }
        list_add_tail(&new_el->list, &q->head);
        q->num_elements++;
    }
    return 0;
}

void vs10xx_queue_free(vs10xx_queue_t *q) {
    unsigned long flags;
    vs10xx_qel_t *el, *tmp;

    spin_lock_irqsave(&q->lock, flags);
    list_for_each_entry_safe(el, tmp, &q->head, list) {
        list_del(&el->list);
        kfree(el);
    }
    q->num_elements = 0;
    spin_unlock_irqrestore(&q->lock, flags);
}

vs10xx_qel_t* vs10xx_queue_get_head(vs10xx_queue_t *q) {
    unsigned long flags;
    vs10xx_qel_t *el = NULL;

    spin_lock_irqsave(&q->lock, flags);
    if (!list_empty(&q->head)) {
        el = list_first_entry(&q->head, vs10xx_qel_t, list);
        list_del(&el->list);
        q->num_elements--;
    }
    spin_unlock_irqrestore(&q->lock, flags);
    return el;
}

void vs10xx_queue_put_tail(vs10xx_queue_t *q, vs10xx_qel_t *el) {
    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);
    list_add_tail(&el->list, &q->head);
    q->num_elements++;
    spin_unlock_irqrestore(&q->lock, flags);
}