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

typedef struct {
    olist_t list;
    pthread_mutex_t lock;
} wentry_t;

typedef struct {
    wentry_t *entries;
    unsigned bits;
} wtable_t;


int wtable_init(wtable_t *wtable, unsigned bits);
unsigned wtable_hash(wtable_t *wtable, char c);
int wtable_insert(wtable_t *wtable, const char *key, void *value);
int wtable_iterate(wtable_t *wtable, olist_iter_t iter, void *user);
void wtable_destroy(wtable_t *wtable);

#endif
