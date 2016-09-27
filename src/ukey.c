#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

#include "utils.h"

#include "ukey.h"

ukey_t *ukey_init(const char *data, size_t len) {
    ukey_t *key;

    if (len >= (typeof(key->len))-1)
        return NULL;

    key = malloc(sizeof(ukey_t)+len+1);
    if (!key)
        return NULL;

    memcpy(key->key, data, len);
    key->key[len] = 0;
    key->ref = 1;
    key->len = len;
    key->hash = fnv1_32(data, len);

    return key;
}

void ukey_get(ukey_t *key) {
    atomic_fetch_add(&key->ref, 1);
}

void ukey_put(ukey_t *key) {
    long val = atomic_fetch_sub(&key->ref, 1);
    assert(val >= 1);
    if (val == 1)
        free(key);
}
