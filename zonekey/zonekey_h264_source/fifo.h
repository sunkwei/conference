#ifndef _fifo_hh
#define _fifo_hh

/** 实现一个简单的 fifo */

#ifdef __cplusplus
extern "C" {
#endif /* c++ */

typedef struct fifo_t fifo_t;

fifo_t *fifo_new();
void fifo_free(fifo_t *);

/** 返回长度 */
int fifo_size(fifo_t *f);

/** 追加 */
void fifo_add(fifo_t *f, void *data);

/** 取出，必须 fifo_size() > 0 */
void *fifo_pop(fifo_t *f);


#ifdef __cplusplus
}
#endif /* c++ */

#endif /* fifo.h */
