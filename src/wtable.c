#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "utils.h"

#include "wtable.h"

/* IMPL */

typedef struct {
    unsigned long pos;
    unsigned shard;
} _sindex_data_t;

typedef struct {
    _sindex_data_t *sindex;
    unsigned long index;
    unsigned shard;
    unsigned max;
} _witer_data_t;

static inline unsigned _bits(unsigned hash, unsigned bits) {
    return hash & ((1 << bits) - 1);
}

static void _witer(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    _witer_data_t *wdata = user;
    _sindex_data_t *cur;

    if (wdata->index >= wdata->max) {
        fprintf(stderr, "Skipping list key '%s'\n", key->key);
        return;
    }

    cur = &wdata->sindex[wdata->index++];
    cur->shard = wdata->shard;
    cur->pos = entries - values;
}

static int __sindex_cmp(const void *_a, const void *_b, void *user) {
    wtable_t *wtable = user;
    const _sindex_data_t *a = _a, *b = _b;
    wentry_t *wa = &wtable->entries[a->shard];
    wentry_t *wb = &wtable->entries[b->shard];
    olentry_t ela, elb;
    int res;

    res = olist_get_entry(&wa->list, a->pos, &ela);
    if (res != 0)
        die("Failed to get element from shard %lu: %d\n", a->pos, res);

    res = olist_get_entry(&wb->list, b->pos, &elb);
    if (res != 0)
        die("Failed to get element from shard %lu: %d\n", b->pos, res);

    res = strcmp(ela.key->key, elb.key->key);
    if (res < 0)
        return -1;
    if (res > 0)
        return 1;
    return 0;
}

/* API */

int wtable_init(wtable_t *wtable, unsigned bits) {
    unsigned size = 1 << bits;
    unsigned idx;
    int err;

    if (!wtable || !bits || bits > 31)
        return -EINVAL;

    memset(wtable, 0, sizeof(wtable_t));
    wtable->entries = calloc(1, sizeof(wentry_t)*size);
    if (!wtable->entries)
        return -ENOMEM;

    for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        err = olist_init(&w->list);
        if (err != 0)
            goto fail;
        err = pthread_rwlock_init(&w->lock, NULL);
        if (err != 0)
            goto fail;
    }
    wtable->bits = bits;

    return 0;

  fail:
    for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        if (!w) continue;
        olist_destroy(&w->list);
        pthread_rwlock_destroy(&w->lock);
    }
    free(wtable->entries);

    return err;
}

int wtable_insert(wtable_t *wtable, ukey_t *key, void *value) {
    if (!wtable || !key)
        return -EINVAL;

    unsigned hval = _bits(key->hash, wtable->bits);
    wentry_t *w = &wtable->entries[hval];
    int err;

    __rw_wrlock(&w->lock);
    err = olist_insert(&w->list, key, value);
    __rw_unlock(&w->lock);

    return err;
}

int wtable_iterate_unordered(wtable_t *wtable, olist_iter_t iter, void *user) {
    if (!wtable)
        return -EINVAL;

    int err;
    const unsigned size = 1 << wtable->bits;
    unsigned idx; for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        __rw_rdlock(&w->lock);
        err = olist_iterate(&w->list, iter, user);
        __rw_unlock(&w->lock);
        if (err != 0)
            return err;
    }

    return 0;
}

int wtable_iterate(wtable_t *wtable, olist_iter_t iter, void *user) {
    if (!wtable)
        return -EINVAL;

    int err = 0;
    const unsigned size = 1 << wtable->bits;
    unsigned long count = 0;
    unsigned locked = 0;
    _sindex_data_t *sindex = NULL;
    unsigned long idx;

    for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        ++locked;
#if !defined(TSAN)
        __rw_rdlock(&w->lock);
#endif
        count += olist_uniq_qty(&w->list);
    }

    if (count == 0)
        goto out;

    sindex = malloc(count * sizeof(_sindex_data_t));
    if (!sindex) {
        err = -ENOMEM;
        goto out;
    }

    _witer_data_t wdata = {
        .sindex = sindex,
        .max = count,
        .index = 0,
    };
    for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        wdata.shard = idx;
        err = olist_iterate(&w->list, _witer, &wdata);
        if (err != 0)
            goto out;
    }
    assert(count == wdata.index);

    qsort_r(sindex, count, sizeof(_sindex_data_t), __sindex_cmp, wtable);

    for (idx=0; idx<count; ++idx) {
        _sindex_data_t *el = &sindex[idx];
        wentry_t *w = &wtable->entries[el->shard];
        olentry_t *values = w->list.values.values;
        olentry_t oel;

        err = olist_get_entry(&w->list, el->pos, &oel);
        if (err != 0)
            break;
        iter(oel.key, &oel, values, user);
    }

  out:
#if !defined(TSAN)
    for (idx=0; idx<locked; ++idx) {
        wentry_t *w = &wtable->entries[locked-idx-1];
        __rw_unlock(&w->lock);
    }
#endif

    free(sindex);

    return err;
}

int wtable_get_entry(wtable_t *wtable, ukey_t *key, unsigned long pos, olentry_t *entry) {
    if (!wtable || !key)
        return -EINVAL;

    unsigned hval = _bits(key->hash, wtable->bits);
    wentry_t *w = &wtable->entries[hval];
    int err;

    __rw_rdlock(&w->lock);
    err = olist_get_entry(&w->list, pos, entry);
    __rw_unlock(&w->lock);

    return err;
}

void wtable_destroy(wtable_t *wtable) {
    if (!wtable) return;

    unsigned size = 1 << wtable->bits;
    unsigned idx;

    for (idx=0; idx<size; ++idx) {
        wentry_t *w = &wtable->entries[idx];
        if (!w) continue;
        olist_destroy(&w->list);
        pthread_rwlock_destroy(&w->lock);
    }
    free(wtable->entries);

    memset(wtable, 0, sizeof(wtable_t));
}
