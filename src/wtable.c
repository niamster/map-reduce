#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wtable.h"

/* IMPL */

#define FNV1_32_INIT 0x811c9dc5

static unsigned _fnv_32(char c, unsigned bits) {
    unsigned hval = FNV1_32_INIT;

    if (c) {
        /* multiply by the 32 bit FNV magic prime mod 2^32 */
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        /* xor the bottom with the current octet */
        hval ^= c;
    }

    return hval & ((1 << bits) - 1);
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

unsigned wtable_hash(wtable_t *wtable, char c) {
    return _fnv_32(c, wtable->bits);
}

int wtable_insert(wtable_t *wtable, ukey_t *key, void *value) {
    if (!wtable || !key)
        return -EINVAL;

    unsigned hval = _fnv_32(key->key[0], wtable->bits);
    wentry_t *w = &wtable->entries[hval];
    int err;

    __rw_wrlock(&w->lock);
    err = olist_insert(&w->list, key, value);
    __rw_unlock(&w->lock);

    return err;
}

int wtable_iterate(wtable_t *wtable, olist_iter_t iter, void *user) {
    if (!wtable)
        return -EINVAL;

    int err;
    unsigned idx; for (idx=0; idx<=255; ++idx) {
        unsigned hval = _fnv_32(idx, wtable->bits);
        wentry_t *w = &wtable->entries[hval];
        __rw_rdlock(&w->lock);
        err = olist_iterate(&w->list, iter, user);
        __rw_unlock(&w->lock);
        if (err != 0)
            return err;
    }

    return 0;
}

int wtable_get_entry(wtable_t *wtable, ukey_t *key, long long pos, olentry_t *entry) {
    if (!wtable || !key)
        return -EINVAL;

    unsigned hval = _fnv_32(key->key[0], wtable->bits);
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
