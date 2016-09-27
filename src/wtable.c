#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

#include "wtable.h"

/* IMPL */

static inline unsigned _bits(unsigned hash, unsigned bits) {
    return hash & ((1 << bits) - 1);
}

static int __sindex_cmp(const void *_a, const void *_b, void *user) {
    wtable_t *wtable = user;
    const unsigned *a = _a, *b = _b;
    wentry_t *wa = &wtable->entries[*a];
    wentry_t *wb = &wtable->entries[*b];
    olentry_t ela, elb;
    int res;

    __rw_rdlock(&wa->lock);
    res = olist_get_entry(&wa->list, 0, &ela);
    __rw_unlock(&wa->lock);
    if (res != 0)
        die("Failed to get element from shard %u: %d\n", *a, res);

    __rw_rdlock(&wb->lock);
    res = olist_get_entry(&wb->list, 0, &elb);
    __rw_unlock(&wb->lock);
    if (res != 0)
        die("Failed to get element from shard %u: %d\n", *b, res);

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

    int err;
    const unsigned size = 1 << wtable->bits;
    unsigned idx;

    /* Just sort the shards, the items in the lists are already sorted. */
    /* TODO: try merge as olist is internally sorted */

    unsigned *sindex = malloc(size * sizeof(unsigned));
    if (!sindex)
        return -ENOMEM;

    unsigned sidx = 0;
    for (idx=0; idx<size; ++idx) {
        unsigned count;
        wentry_t *w = &wtable->entries[idx];
        __rw_rdlock(&w->lock);
        count = w->list.index.count;
        __rw_unlock(&w->lock);
        if (count == 0)
            continue;
        sindex[sidx++] = idx;
    }

    qsort_r(sindex, sidx, sizeof(unsigned), __sindex_cmp, wtable);

    for (idx=0; idx<sidx; ++idx) {
        wentry_t *w = &wtable->entries[sindex[idx]];
        __rw_rdlock(&w->lock);
        err = olist_iterate(&w->list, iter, user);
        __rw_unlock(&w->lock);
        if (err != 0)
            return err;
    }


    free(sindex);

    return 0;
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
