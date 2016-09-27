#ifndef _MR_WTABLE_H_
#define _MR_WTABLE_H_

/* Wtable is a "distributed" word table.
 * It's like a normal hash table but only first byte is taken for hash calculation.
 * Since it's only one byte of information FNV1 hash was chosen as it provides good distribution.
 * The study on the hash functions: http://programmers.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed
 * Reference implementation of FNV1 was taken from http://www.isthe.com/chongo/src/fnv/hash_32.c
 */

#include "utils.h"
#include "olist.h"

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    olist_t list;
    pthread_rwlock_t lock;
} wentry_t;

typedef struct {
    wentry_t *entries;
    unsigned bits;
} wtable_t;


int wtable_init(wtable_t *wtable, unsigned bits);
int wtable_insert(wtable_t *wtable, ukey_t *key, void *value);
int wtable_iterate(wtable_t *wtable, olist_iter_t iter, void *user);
int wtable_iterate_unordered(wtable_t *wtable, olist_iter_t iter, void *user);
int wtable_get_entry(wtable_t *wtable, ukey_t *key, long long pos, olentry_t *entry);
void wtable_destroy(wtable_t *wtable);

#ifdef __cplusplus
};
#endif

#endif
