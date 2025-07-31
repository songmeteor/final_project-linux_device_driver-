#ifndef VS1003QUEUE_H
#define VS1003_QUEUE_H

struct vs1003_queue_buf_t {
// up to 32 bytes can be written
// without checking for dreq
	char data[32];
	unsigned len;
};

int vs1003_queue_init(int id);
void vs1003_queue_exit(int id);
void vs1003_queue_flush(int id);

int vs1003_queue_isfull(int id);
int vs1003_queue_isempty(int id);
int vs1003_queue_getfree(int id);

struct vs1003_queue_buf_t* vs1003_queue_getslot(int id);
struct vs1003_queue_buf_t* vs1003_queue_gethead(int id);

void vs1003_queue_enqueue(int id);
void vs1003_queue_dequeue(int id);

#endif