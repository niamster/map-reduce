#ifndef _MR_OLIST_H_
#define _MR_OLIST_H_

/* Simple ordered list with dups */

typedef struct {
    const char *key;
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

typedef void (*olist_iter_t)(const char *key, olentry_t *entries, olentry_t *values, void *user);

int olist_init(olist_t *olist);
int olist_insert(olist_t *olist, const char *key, void *value);
int olist_iterate(olist_t *olist, olist_iter_t iter, void *user);
void olist_destroy(olist_t *olist);

#endif
