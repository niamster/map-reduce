#ifndef _MR_UKEY_H_
#define _MR_UKEY_H_

#include <sys/types.h>
#include <stdlib.h>

#ifdef __cplusplus
#include <atomic>
using namespace std;
extern "C" {
#else
#include <stdatomic.h>
#endif

typedef struct {
    size_t len;
    atomic_uint ref;
    char key[]; // contains '\0'
} ukey_t;

ukey_t *ukey_init(const char *data, size_t len);
void ukey_get(ukey_t *key);
void ukey_put(ukey_t *key);

#ifdef __cplusplus
};
#endif

#endif
