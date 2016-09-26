#ifndef _MR_MAPRED_H_
#define _MR_MAPRED_H_

#include "wtable.h"
#include "ukey.h"
#include "tp.h"

typedef struct {
    wtable_t table;
    tp_t tp;
    unsigned threads;
} mr_t;

typedef void (*mr_map_cb_t)(mr_t *mr, ukey_t *key, void *user);
typedef void (*mr_reduce_cb_t)(mr_t *mr, ukey_t *key, olentry_t *entries, olentry_t *values, void *user);

int mr_init(mr_t *mr, unsigned threads);
int mr_process_fd(mr_t *mr, int fd, mr_map_cb_t map, mr_reduce_cb_t reduce, void *user);
int mr_process(mr_t *mr, const char *mem, size_t size, mr_map_cb_t map, mr_reduce_cb_t reduce, void *user);
int mr_emit(mr_t *mr, ukey_t *key, void *value);
void mr_destroy(mr_t *mr);

#endif
