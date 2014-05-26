#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include "fifo.h"

typedef struct fifo
{
	void *data_;
	struct fifo *next;
} fifo;

fifo_t *fifo_new()
{
	fifo *f = (fifo*)malloc(sizeof(fifo));
	f->next = 0;
	return (fifo_t*)f;
}

void fifo_free(fifo_t *x)
{
	fifo *f = (fifo*)x;
	free(f);
}

int fifo_size(fifo_t *x)
{
	fifo *f = (fifo*)x;
	fifo *curr = f->next;
	int size = 0;
	while (curr) {
		size++;
		curr = curr->next;
	}
	return size;
}

/** 添加新的节点，总是就近添加 */
void fifo_add(fifo_t *x, void *data)
{
	fifo *f = (fifo*)x;
	fifo *n = (fifo*)malloc(sizeof(fifo));
	n->data_ = data;
	n->next = f->next;
	f->next = n;
}

/** 提取最老的 */
void *fifo_pop(fifo_t *x)
{
	fifo *f = (fifo*)x;
	void *data;

	assert(f->next);	// 必须有元素！！！

	// 找到倒数第二个
	while (f->next->next) {
		f = f->next;
	}

	data = f->next->data_;
	free(f->next);
	f->next = 0;

	return data;
}
