#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

#include "tp.h"

/* IMPL */

typedef struct {
    tp_cb_t tsk;
    void *user;
    dllist_t list;
} tp_task_t;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    bool done;
} _wake_t;

static void __lock(tp_t *tp) {
    __mtx_lock(&tp->lock);
}

static void __unlock(tp_t *tp) {
    __mtx_unlock(&tp->lock);
}

static void __wait(tp_t *tp) {
    __cond_wait(&tp->cond, &tp->lock);
}

static void __notify(tp_t *tp) {
    __cond_notify(&tp->cond);
}

static void __broadcast(tp_t *tp) {
    __cond_broadcast(&tp->cond);
}

static int __tp_push(tp_t *tp, tp_cb_t tsk, void *user) {
    tp_task_t *task;

    task = calloc(1, sizeof(tp_task_t));
    if (!task)
        return -ENOMEM;

    task->tsk = tsk;
    task->user = user;
    __lock(tp);
    dllist_add_tail(&tp->tasks, &task->list);
    __notify(tp);
    __unlock(tp);

    return 0;
}

static void *__worker(void *data) {
    tp_t_t *me = data;
    tp_t *tp = me->tp;

    while (true) {
        dllist_t *node = NULL;
        tp_task_t *task = NULL;
        bool should_stop = false;
        __lock(tp);
        while (dllist_empty(&tp->tasks) && !me->should_stop)
            __wait(tp);
        if (!me->should_stop) {
            node = dllist_first(&tp->tasks);
            dllist_detach(node);
            __sync_add_and_fetch(&me->active, 1);
        } else
            should_stop = true;
        __unlock(tp);

        if (should_stop)
            break;

        task = container_of(node, tp_task_t, list);
        task->tsk(task->user);

        __sync_sub_and_fetch(&me->active, 1);
        free(task);
    }

    return NULL;
}

static void __wake_sync(void *user) {
    _wake_t *wake = user;


    assert(pthread_mutex_lock(&wake->mtx) == 0);
    wake->done = true;
    assert(pthread_cond_signal(&wake->cond) == 0);
    assert(pthread_mutex_unlock(&wake->mtx) == 0);
}

static int __stop(tp_t *tp, unsigned count) {
    unsigned idx;
    int err;

    if (!tp || count > tp->t_max)
        return -EINVAL;

    unsigned delta = tp->t_max - count;

    __lock(tp);
    tp->state = TP_ST_INACTIVE;

    for (idx=delta; idx<tp->t_max; ++idx) {
        tp_t_t *t = &tp->threads[idx];
        t->should_stop = true;
    }
    __broadcast(tp);
    __unlock(tp);

    for (idx=delta; idx<tp->t_max; ++idx) {
        tp_t_t *t = &tp->threads[idx];
        err = pthread_join(t->thread, NULL);
        if (err != 0)
            return err;
        memset(t, 0, sizeof(tp_t_t));
    }
    tp->t_max -= count;

    __lock(tp);
    tp->state = TP_ST_ACTIVE;
    __unlock(tp);

    return 0;
}


/* API */

int tp_init(tp_t *tp, unsigned threads) {
    int err;

    if (!tp || !threads)
        return -EINVAL;

    memset(tp, 0, sizeof(tp_t));
    dllist_init(&tp->tasks);

    err = pthread_mutex_init_ec(&tp->lock);
    if (err != 0)
        return err;

    err = pthread_cond_init(&tp->cond, NULL);
    if (err != 0)
        goto fail;

    tp->threads = calloc(1, sizeof(tp_t_t)*threads);
    if (!tp->threads) {
        err = -ENOMEM;
        goto fail;
    }
    tp->t_max = threads;

    unsigned idx; for (idx=0; idx<threads; ++idx) {
        tp_t_t *t = &tp->threads[idx];
        t->tp = tp;
        err = pthread_create(&t->thread, NULL, __worker, t);
        if (err != 0)
            goto fail;
    }
    tp->state = TP_ST_ACTIVE;

    return 0;

  fail:
    tp_destroy(tp);

    return err;
}

int tp_set_threads(tp_t *tp, unsigned threads) {
    if (!tp)
        return -EINVAL;

    if (threads < tp->t_max)
        return __stop(tp, tp->t_max - threads);

    assert(0 && "Not supported");

    return 0;
}

int tp_push(tp_t *tp, tp_cb_t tsk, void *user) {
    tp_st_t state;

    if (!tp || !tsk)
        return -EINVAL;

    __lock(tp);
    state = tp->state;
    __unlock(tp);

    if (state != TP_ST_ACTIVE)
        return -EBUSY;

    return __tp_push(tp, tsk, user);
}

int tp_sync(tp_t *tp) {
    tp_st_t state;
    int err = 0;
    _wake_t wake;

    __lock(tp);
    state = tp->state;
    tp->state = TP_ST_SYNC;
    __unlock(tp);

    err = pthread_mutex_init_ec(&wake.mtx);
    if (err != 0)
        goto out;
    err = pthread_cond_init(&wake.cond, NULL);
    if (err != 0)
        goto out;

  retry:
    wake.done = false;
    err = __tp_push(tp, __wake_sync, &wake);
    if (err != 0)
        goto out;

    bool done = false;
    while (!done) {
        err = pthread_mutex_lock(&wake.mtx);
        if (err != 0)
            break;
        done = wake.done;
        if (!done) {
            err = pthread_cond_wait(&wake.cond, &wake.mtx);
            if (err != 0)
                break;
        }
        done = wake.done;
        err = pthread_mutex_unlock(&wake.mtx);
        if (err != 0)
            break;

        /* The queue is empty but another thread might still be running. */
        unsigned idx; for (idx=0; idx<tp->t_max; ++idx) {
            tp_t_t *t = &tp->threads[idx];
            if (__sync_sub_and_fetch(&t->active, 0))
                goto retry;
        }
    }

  out:
    __lock(tp);
    if (err == 0)
        assert(dllist_empty(&tp->tasks));
    tp->state = state;
    __unlock(tp);

    pthread_mutex_destroy(&wake.mtx);
    pthread_cond_destroy(&wake.cond);

    return err;
}

void tp_destroy(tp_t *tp) {
    int err;

    if (!tp)
        return;

    err = __stop(tp, tp->t_max);
    if (err != 0)
        die("Failed to stop tasks: %s\n", strerror(err));

    free(tp->threads), tp->threads = NULL;

    err = pthread_mutex_destroy(&tp->lock);
    if (err != 0)
        die("Failed to destroy mutex: %s\n", strerror(err));
    err = pthread_cond_destroy(&tp->cond);
    if (err != 0)
        die("Failed to destroy mutex: %s\n", strerror(err));

    tp->state = TP_ST_INIT;
}
