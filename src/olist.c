#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "olist.h"

#if !defined(OLIST_INIT_SIZE)
#define OLIST_INIT_SIZE 0
#endif

/* IMPL */

static int ____resize(olist_storage_t *storage, unsigned elsize, unsigned count) {
    void *elements;

    if (count <= storage->size)
        return 0;

    elements = realloc(storage->elements, count*elsize);
    if (!elements)
        return -ENOMEM;

    storage->elements = elements;
    storage->size = count;

    return 0;
}

static int __resize(olist_storage_t *storage, unsigned elsize) {
    if (storage->size >= storage->count + 1)
        return 0;

    unsigned nsize = (storage->size?:1) * 2;

    return ____resize(storage, elsize, nsize);
}

static int __resize_storage(olist_t *olist) {
     return __resize(&olist->values, sizeof(olentry_t));
}

static int __resize_index(olist_t *olist) {
    return __resize(&olist->index, sizeof(long long));
}

static int __insert(olist_t *olist, const long long pos) {
    olentry_t *values = olist->values.values;
    olentry_t *val = &olist->values.values[pos];
    long long *index;
    long long left, right;
    long long tgt = 0;
    int res;

    if (0 == olist->index.count)
        goto insert;

    left = 0;
    right = olist->index.count - 1;
    index = olist->index.index;
    while (left <= right) {
        tgt = (left + right) / 2;
        long long epos = index[tgt];
        olentry_t *el = &values[epos];

        int cmp = strcmp(val->key->key, el->key->key);
        if (cmp == 0) { // dup
            val->next = epos;
            index[tgt] = pos;
            return 0;
        }

        if (cmp < 0)
            right = tgt - 1;
        else {
            ++tgt; // advance for insert case
            left = tgt;
        }
    }

  insert:
    res = __resize_index(olist);
    if (res != 0)
        return res;
    index = olist->index.index;
    if (olist->index.count > 0)
        memmove(index + tgt + 1, index + tgt, sizeof(olentry_t *) * (olist->index.count-tgt));
    ++olist->index.count;
    index[tgt] = pos;

    return 0;
}

/* API */

int olist_init(olist_t *olist) {
    int res;

    if (!olist)
        return -EINVAL;

    memset(olist, 0, sizeof(olist_t));
    res = ____resize(&olist->values, sizeof(olentry_t), OLIST_INIT_SIZE);
    if (res != 0)
        return res;
    res = ____resize(&olist->index, sizeof(long long), OLIST_INIT_SIZE);
    if (res != 0)
        goto fail;

    return 0;

  fail:
    olist_destroy(olist);
    return res;
}

int olist_insert(olist_t *olist, ukey_t *key, void *value) {
    olentry_t *val;
    unsigned pos;
    int res;

    if (!olist || !key)
        return -EINVAL;

    res = __resize_storage(olist);
    if (res != 0)
        return res;

    pos = olist->values.count++;

    val = &olist->values.values[pos];
    val->key = key;
    val->value = value;
    val->next = -1;
    ukey_get(key);

    res = __insert(olist, pos);
    if (res != 0)
        ukey_put(key);

    return res;
}

int olist_iterate(olist_t *olist, olist_iter_t iter, void *user) {
    unsigned idx;

    if (!olist || !iter)
        return -EINVAL;

    olist_storage_t *index = &olist->index;
    olentry_t *values = olist->values.values;
    for (idx=0; idx<index->count; ++idx) {
        long long epos = index->index[idx];
        olentry_t *el = &values[epos];
        iter(el->key, el, olist->values.values, user);
    }

    return 0;
}

int olist_get_entry(olist_t *olist, long long pos, olentry_t *entry) {
    if (!olist || !entry)
        return -EINVAL;

    *entry = olist->values.values[pos];

    return 0;
}

void olist_destroy(olist_t *olist) {
    unsigned idx; for (idx=0; idx<olist->values.count; ++idx) {
        olentry_t *el = &olist->values.values[idx];
        ukey_put(el->key);
    }

    free(olist->values.elements);
    free(olist->index.elements);

    memset(olist, 0, sizeof(olist_t));
}
