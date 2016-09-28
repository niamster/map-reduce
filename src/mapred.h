#ifndef _MR_MAPRED_H_
#define _MR_MAPRED_H_

#include "wtable.h"
#include "ukey.h"
#include "tp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wtable_t input;
    wtable_t *output;
    tp_t tp;
    unsigned threads;
} mr_t;

typedef void (*mr_map_cb_t)(mr_t *mr, const char *data, size_t size, void *user);
typedef void (*mr_reduce_cb_t)(mr_t *mr, ukey_t *key, unsigned long entry, void *user);
typedef void (*mr_output_cb_t)(ukey_t *key, olentry_t *entry, void *user);

int mr_init(mr_t *mr, unsigned threads);
int mr_set_threads(mr_t *mr, unsigned threads);
int mr_process_fd(mr_t *mr, int fd, mr_map_cb_t map, mr_reduce_cb_t reduce, mr_output_cb_t output, void *user);
int mr_process(mr_t *mr, const char *mem, size_t size, mr_map_cb_t map, mr_reduce_cb_t reduce, mr_output_cb_t output, void *user);
int mr_emit_intermediate(mr_t *mr, ukey_t *key, void *value);
int mr_emit(mr_t *mr, ukey_t *key, void *value);
int mr_get_entry(mr_t *mr, ukey_t *key, unsigned long pos, olentry_t *entry);
void mr_destroy(mr_t *mr);

#ifdef __cplusplus
};
#endif

#endif
