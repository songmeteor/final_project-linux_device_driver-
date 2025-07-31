#include "vs1003_common.h"
#include "vs1003_queue.h"

#include <linux/slab.h>
#include <linux/moduleparam.h>

/* queue size*/
static int queuelen = 2048;
module_param(queuelen, int, 0644);

struct vs1003_queue_entry_t {
	struct list_head list;
	struct vs1003_queue_buf_t buf;
};

struct vs1003_queue_t {
	int id;
	int free;
	int valid;
	struct list_head buf_pool;
	struct list_head buf_data;
};

static struct vs1003_queue_t vs1003_queue[VS1003_MAX_DEVICES];

/* ----------------------------------------------------------------------------------------------------------------------------- */
/* VS1003 QUEUE BUFFERS                                                                                                          */
/* ----------------------------------------------------------------------------------------------------------------------------- */

static int vs1003_queue_alloc(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	struct vs1003_queue_entry_t *entry;
	int i, status = 0;

	/* create buf_pool list */
	INIT_LIST_HEAD(&queue->buf_pool);

	/* create buf_data list */
	INIT_LIST_HEAD(&queue->buf_data);

	/* allocate buffers in pool */
	for(i = 0; (status == 0) && (i < queuelen); i++) {
		entry = kzalloc(sizeof *entry, GFP_KERNEL);
		if (!entry) {
			vs1003_err("kzalloc queue entry error.\n");
			status = -1;
		} else {
			list_add(&entry->list, &queue->buf_pool);
		}
	}

	queue->free = ((status == 0) ? queuelen : 0);
	queue->valid = ((status == 0) ? 1 : 0);

	vs1003_inf("id:%d allocated %d/%d Pool buffers (%d bytes)", queue->id, i, queuelen, (i * sizeof(entry->buf)));

	return status;
}

void vs1003_queue_flush(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	struct vs1003_queue_entry_t *entry, *n;

	/* move all buffers to pool */
	if (!list_empty(&queue->buf_data)) {
		vs1003_dbg("id:%d flush queue", id);
		list_for_each_entry_safe(entry, n, &queue->buf_data, list) {
			list_move_tail(&entry->list, &queue->buf_pool);
		}
	}
}

static void vs1003_queue_free(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	struct vs1003_queue_entry_t *entry, *n;

	if (queue->valid) {

		vs1003_queue_flush(id);

		/* free buffers */
		list_for_each_entry_safe(entry, n, &queue->buf_pool, list) {
			list_del(&entry->list);
			kfree(entry);
		}

		queue->free = 0;
		queue->valid = 0;
	}
}

int vs1003_queue_isfull(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];

	return list_empty(&queue->buf_pool);

}

int vs1003_queue_isempty(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];

	return list_empty(&queue->buf_data);

}

int vs1003_queue_getfree(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];

	return queue->free;
}

struct vs1003_queue_buf_t* vs1003_queue_getslot(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	/* get the first (empty) data entry from the data queue */
	struct vs1003_queue_entry_t *entry = list_first_entry(&queue->buf_pool, struct vs1003_queue_entry_t, list);

	return &entry->buf;
}

struct vs1003_queue_buf_t* vs1003_queue_gethead(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	/* get the first (filled) data entry from the data queue */
	struct vs1003_queue_entry_t *entry = list_first_entry(&queue->buf_data, struct vs1003_queue_entry_t, list);

	return &entry->buf;
}

void vs1003_queue_enqueue(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	/* could ask the caller for the entry, but let's do not to prevent queue corruption */
	struct vs1003_queue_entry_t *entry = list_first_entry(&queue->buf_pool, struct vs1003_queue_entry_t, list);
	if (entry->buf.len > 0) {
		list_move_tail(&entry->list, &queue->buf_data);
		queue->free -= 1;
	}
}

void vs1003_queue_dequeue(int id) {

	struct vs1003_queue_t *queue = &vs1003_queue[id];
	/* could ask the caller for the entry, but let's do not to prevent queue corruption */
	struct vs1003_queue_entry_t *entry = list_first_entry(&queue->buf_data, struct vs1003_queue_entry_t, list);
	list_move_tail(&entry->list, &queue->buf_pool);
	entry->buf.len = 0;
	queue->free += 1;
}

/* ----------------------------------------------------------------------------------------------------------------------------- */
/* VS1003 QUEUE INIT/EXIT                                                                                                        */
/* ----------------------------------------------------------------------------------------------------------------------------- */

int vs1003_queue_init(int id) {

	int status;

	vs1003_queue[id].id = id;
	status = vs1003_queue_alloc(id);

	return status;
}

void vs1003_queue_exit(int id) {

	vs1003_queue_free(id);

}