#ifndef _MR_OLIST_H_
#define _MR_OLIST_H_

#include "ukey.h"

/* Simple ordered list with dups */

typedef struct {
    ukey_t *key;
    void *value;
    long long next; // position in the storage, -1 means the end
} olentry_t;

typedef struct {
    union {
        olentry_t *values;
        long long *index;
        void *elements;
    };
    unsigned count; // number of values
    unsigned size; // storage capacity
} olist_storage_t;

typedef struct {
    olist_storage_t values;
    olist_storage_t index;
} olist_t;

/* @entries is a pointer to the first element for a given key. */
/* @values is a pointer ot the olist values. Used to access next element by offset. */
typedef void (*olist_iter_t)(ukey_t *key, olentry_t *entries, olentry_t *values, void *user);

int olist_init(olist_t *olist);
int olist_insert(olist_t *olist, ukey_t *key, void *value);
int olist_iterate(olist_t *olist, olist_iter_t iter, void *user);
void olist_destroy(olist_t *olist);

#endif
