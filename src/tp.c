#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

#include "tp.h"

/* IMPL */

typedef enum {
    TP_TA_EXE,
    TP_TA_STOP,
} tp_ta_t;

typedef struct {
    tp_cb_t tsk;
    void *user;
    tp_ta_t action;
    dllist_t list;
} tp_task_t;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    bool done;
} _wake_t;

static void __lock(tp_t *tp) {
    int err = pthread_mutex_lock(&tp->lock);
    if (err != 0)
        die("Failed to lock TP: %s", strerror(err));
}

static void __unlock(tp_t *tp) {
    int err = pthread_mutex_unlock(&tp->lock);
    if (err != 0)
        die("Failed to unlock TP: %s", strerror(err));
}

static void __wait(tp_t *tp) {
    int err = pthread_cond_wait(&tp->cond, &tp->lock);
    if (err != 0)
        die("Failed to wait on TP: %s", strerror(err));
}

static void __notify(tp_t *tp) {
    int err = pthread_cond_signal(&tp->cond);
    if (err != 0)
        die("Failed to signal on TP: %s", strerror(err));
}

static int __tp_push(tp_t *tp, tp_cb_t tsk, void *user, tp_ta_t action) {
    tp_task_t *task;

    task = calloc(1, sizeof(tp_task_t));
    if (!task)
        return -ENOMEM;

    task->tsk = tsk;
    task->user = user;
    task->action = action;
    __lock(tp);
    dllist_add_tail(&tp->tasks, &task->list);
    __notify(tp);
    __unlock(tp);

    return 0;
}

static void __wake_sync(void *user);
static void *__worker(void *data) {
    tp_t *tp = data;
    bool running = true;

    while (running) {
        dllist_t *node = NULL;
        tp_task_t *task = NULL;
        __lock(tp);
        while (dllist_empty(&tp->tasks))
            __wait(tp);
        node = dllist_first(&tp->tasks);
        dllist_detach(node);
        __unlock(tp);

        task = container_of(node, tp_task_t, list);
        switch (task->action) {
            case TP_TA_EXE:
                task->tsk(task->user);
                break;

            case TP_TA_STOP:
                running = false;
                break;
        }

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


/* API */

int tp_init(tp_t *tp, unsigned threads) {
    pthread_mutexattr_t mattr;
    int err;

    if (!tp || !threads)
        return -EINVAL;

    memset(tp, 0, sizeof(tp_t));
    dllist_init(&tp->tasks);

    err = pthread_mutexattr_init(&mattr);
    if (err != 0)
        return err;
    err = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
    if (err != 0)
        goto fail;
    err = pthread_mutex_init(&tp->lock, &mattr);
    if (err != 0)
        goto fail;

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
        err = pthread_create(&t->thread, NULL, __worker, tp);
        if (err != 0)
            goto fail;
    }
    tp->state = TP_ST_ACTIVE;

    pthread_mutexattr_destroy(&mattr);

    return 0;

  fail:
    pthread_mutexattr_destroy(&mattr);
    tp_destroy(tp);

    return err;
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

    return __tp_push(tp, tsk, user, TP_TA_EXE);
}

int tp_sync(tp_t *tp) {
    tp_st_t state;
    int err = 0;
    _wake_t wake;

    __lock(tp);
    state = tp->state;
    tp->state = TP_ST_SYNC;
    __unlock(tp);

    err = pthread_mutex_init(&wake.mtx, NULL);
    if (err != 0)
        goto out;
    err = pthread_cond_init(&wake.cond, NULL);
    if (err != 0)
        goto out;
    wake.done = false;

    err = __tp_push(tp, __wake_sync, &wake, TP_TA_EXE);
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
    unsigned idx;
    int err;

    if (!tp)
        return;

    __lock(tp);
    tp->state = TP_ST_INACTIVE;
    __unlock(tp);

    for (idx=0; idx<tp->t_max; ++idx) {
        err = __tp_push(tp, NULL, NULL, TP_TA_STOP);
        if (err != 0)
            die("Failed to push task: %s\n", strerror(err));
    }
    for (idx=0; idx<tp->t_max; ++idx) {
        tp_t_t *t = &tp->threads[idx];
        err = pthread_join(t->thread, NULL);
        if (err != 0)
            die("Failed to join thread: %s\n", strerror(err));
    }
    free(tp->threads), tp->threads = NULL;

    err = pthread_mutex_destroy(&tp->lock);
    if (err != 0)
        die("Failed to destroy mutex: %s\n", strerror(err));
    err = pthread_cond_destroy(&tp->cond);
    if (err != 0)
        die("Failed to destroy mutex: %s\n", strerror(err));

    tp->state = TP_ST_INIT;
}
