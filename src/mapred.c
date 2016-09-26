#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "mapred.h"

#if !defined(MR_TABLE_BITS)
#define MR_TABLE_BITS 8
#endif

/* IMPL */

typedef struct {
    mr_t *mr;
    mr_map_cb_t map;
    mr_reduce_cb_t reduce;
    mr_output_cb_t output;
    void *user;
} __mr_data_t;

typedef struct {
    __mr_data_t *mdata;
    fchunk_t chunk;
} __mr_chunk_data_t;

typedef struct {
    __mr_data_t *mdata;
    ukey_t *key;
    olentry_t *entries;
    olentry_t *values;
} __mr_iter_data_t;

static void _mr_output(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    __mr_data_t *mdata = user;
    olentry_t el = *entries;
    (void)values;

    el.next = -1;

    mdata->output(key, &el, mdata->user);
}

static void _mr_iter_task(void *user) {
    __mr_iter_data_t *idata = user;
    __mr_data_t *mdata = idata->mdata;

    mdata->reduce(mdata->mr, idata->key, idata->entries, idata->values, mdata->user);

    free(idata);
}

static void __mr_iter(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    __mr_data_t *mdata = user;
    mr_t *mr = mdata->mr;
    __mr_iter_data_t *idata;
    int err;

    idata = calloc(1, sizeof(__mr_iter_data_t));
    if (!idata)
        die("Can't allocate idata\n");
    assert(idata);
    idata->mdata = mdata;
    idata->key = key;
    idata->entries = entries;
    idata->values = values;

    err = tp_push(&mr->tp, _mr_iter_task, idata);
    if (err != 0)
        die("Failed to push chunk process task: %d", err);
}

static void _mr_chunk_process_word(fchunk_t *chunk, void *user) {
    __mr_chunk_data_t *mcdata = user;
    __mr_data_t *mdata = mcdata->mdata;
    ukey_t *key;

    key = ukey_init(chunk->mem+chunk->offset, chunk->count);
    if (!key)
        die("Can't init key\n");
    mdata->map(mdata->mr, key, mdata->user);
    ukey_put(key);
}

static void _mr_chunk_process_task(void *user) {
    __mr_chunk_data_t *mcdata = user;
    fchunk_t *chunk = &mcdata->chunk;

    fchunk_read_word(chunk->mem+chunk->offset, chunk->count,
        _mr_chunk_process_word, mcdata);

    free(mcdata);
}

static void _mr_chunk_process(fchunk_t *chunk, void *user) {
    __mr_data_t *mdata = user;
    mr_t *mr = mdata->mr;
    __mr_chunk_data_t *mcdata;
    int err;

    mcdata = calloc(1, sizeof(__mr_chunk_data_t));
    if (!mcdata)
        die("Can't allocate mcdata\n");
    assert(mcdata);
    mcdata->mdata = mdata;
    mcdata->chunk = *chunk;

    err = tp_push(&mr->tp, _mr_chunk_process_task, mcdata);
    if (err != 0)
        die("Failed to push chunk process task: %d", err);
}

/* API */

int mr_init(mr_t *mr, unsigned threads) {
    int err;
    if (!mr)
        return -EINVAL;

    err = wtable_init(&mr->input, MR_TABLE_BITS);
    if (err != 0)
        return err;

    err = tp_init(&mr->tp, threads);
    if (err != 0)
        goto fail;

    mr->threads = threads;

    return 0;

  fail:
    mr_destroy(mr);
    return err;
}

int mr_process_fd(mr_t *mr, int fd, mr_map_cb_t map, mr_reduce_cb_t reduce, mr_output_cb_t output, void *user) {
    __mr_data_t mdata = {
        .mr = mr,
        .map = map,
        .reduce = reduce,
        .output = output,
        .user = user,
    };
    void *mem;
    size_t len;
    int err;

    err = fchunk_read_fd(fd, mr->threads, _mr_chunk_process, &mdata, &mem, &len);
    if (err != 0)
        return err;

    err = tp_sync(&mr->tp);
    if (err != 0)
        goto out;

    err = wtable_iterate(&mr->input, __mr_iter, &mdata);
    if (err != 0)
        goto out;

    err = tp_sync(&mr->tp);
    if (err != 0)
        goto out;

    err = wtable_iterate(&mr->input, _mr_output, &mdata);

  out:
    munmap(mem, len);

    return err;
}

int mr_process(mr_t *mr, const char *mem, size_t size, mr_map_cb_t map, mr_reduce_cb_t reduce, mr_output_cb_t output, void *user) {
    __mr_data_t mdata = {
        .mr = mr,
        .map = map,
        .reduce = reduce,
        .output = output,
        .user = user,
    };
    int err;

    err = fchunk_read(mem, size, mr->threads, _mr_chunk_process, &mdata);
    if (err != 0)
        return err;

    err = tp_sync(&mr->tp);
    if (err != 0)
        return err;

    err = wtable_iterate(&mr->input, __mr_iter, &mdata);
    if (err != 0)
        return err;

    err = tp_sync(&mr->tp);
    if (err != 0)
        return err;

    return wtable_iterate(&mr->input, _mr_output, &mdata);
}

int mr_emit_intermediate(mr_t *mr, ukey_t *key, void *value) {
    if (!mr)
        return -EINVAL;

    return wtable_insert(&mr->input, key, value);
}

int mr_emit(mr_t *mr, ukey_t *key, void *value) {
    if (!mr)
        return -EINVAL;

    /* There's a tiny hack build on that wtable pushes the value on top of the stack,
     * so if 'output' task touches only the first value - we are safe.
     */
    return wtable_insert(&mr->input, key, value);
}

void mr_destroy(mr_t *mr) {
    if (!mr)
        return;

    wtable_destroy(&mr->input);
    tp_destroy(&mr->tp);

    memset(mr, 0, sizeof(mr_t));
}
