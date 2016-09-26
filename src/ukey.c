#include <stdlib.h>
#include <string.h>

#include "ukey.h"

ukey_t *ukey_init(const char *data, size_t len) {
    ukey_t *key;

    key = calloc(1, sizeof(ukey_t)+len+1);
    if (!key)
        return NULL;

    memcpy(key->key, data, len);
    key->ref = 1;
    key->len = len;

    return key;
}
