#ifndef _MR_UKEY_H_
#define _MR_UKEY_H_

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

typedef struct {
    long ref;
    size_t len;
    char key[]; // contains '\0'
} ukey_t;

ukey_t *ukey_init(const char *data, size_t len);

static inline void ukey_get(ukey_t *key) {
    __sync_add_and_fetch(&key->ref, 1);
}

static inline void ukey_put(ukey_t *key) {
    long val = __sync_sub_and_fetch(&key->ref, 1);
    assert(val >= 0);
    if (val == 0)
        free(key);
}

#endif
