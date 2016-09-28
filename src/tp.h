#ifndef _MR_TP_H_
#define _MR_TP_H_

#include <pthread.h>

#include "dllist.h"

/**
 * Simple thread pool
 * No balancing, no dynamic resize, no data-ALU locality features, single task queue
 */

typedef enum {
    TP_ST_INIT,
    TP_ST_ACTIVE,
    TP_ST_SYNC,
    TP_ST_INACTIVE,
} tp_st_t;

struct tp;
typedef struct tp tp_t;

typedef struct {
    unsigned long active;
    pthread_t thread;
    bool should_stop;
    tp_t *tp;
} tp_t_t;

typedef struct tp {
    tp_st_t state;
    dllist_t tasks;
    unsigned t_max;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    tp_t_t *threads;
} tp_t;

typedef void (*tp_cb_t)(void *user);

int tp_init(tp_t *tp, unsigned threads);
int tp_set_threads(tp_t *tp, unsigned threads);
int tp_push(tp_t *tp, tp_cb_t tsk, void *user);
int tp_sync(tp_t *tp); // waits for completion of pending tasks, new tasks are not accepted
void tp_destroy(tp_t *tp);

#endif
