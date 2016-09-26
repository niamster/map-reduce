#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "utils.h"

void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    assert(0);
}

bool is_sep(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '!':
        case ',':
        case '-':
        case '.':
        case ':':
        case ';':
        case '?':
        case '\0':
            return true;
    }

    return false;
}

int pthread_mutex_init_ec(pthread_mutex_t *mtx) {
    pthread_mutexattr_t mattr;
    int err;

    err = pthread_mutexattr_init(&mattr);
    if (err != 0)
        return err;
    err = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
    if (err != 0)
        goto fail;

    err = pthread_mutex_init(mtx, &mattr);

  fail:
    pthread_mutexattr_destroy(&mattr);

    return err;
}

int fchunk_read_fd(int fd, unsigned max, fchunk_cb_t cb, void *user, void **map, size_t *len) {
    off_t fsize;
    char *mem;
    int ret;

    if (fd < 0)
        return -EINVAL;

    if ((map && !len) || (len && !map))
        return -EINVAL;

    fsize = lseek(fd, 0, SEEK_END);
    if (fsize < 0)
        return -errno;

    mem = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!mem)
        return -errno;

    ret = fchunk_read(mem, fsize, max, cb, user);
    if (map && ret == 0) {
        *map = mem;
        *len = fsize;
    } else
        munmap(mem, fsize);

    return ret;
}

int fchunk_read(const char *mem, size_t size, unsigned max,
    fchunk_cb_t cb, void *user) {
    size_t csize, offset;
    unsigned idx;

    if (!mem || !cb || max == 0)
        return -EINVAL;

    csize = size/max;
    if (csize == 0)
        csize = 1;

    offset = 0;
    for (idx=0; idx<max; ++idx) {
        fchunk_t chunk = {
            .offset = offset,
            .count = MIN(csize, size-offset),
            .index = idx,
            .mem = mem,
        };
        while (chunk.offset + chunk.count < size) {
            char c = mem[chunk.offset + chunk.count];
            if (is_sep(c))
                break;
            ++chunk.count;
        }
        offset += chunk.count;
        cb(&chunk, user);
        if (offset >= size)
            break;
    }

    return 0;
}

void fchunk_read_word(const char *mem, size_t size,
    fchunk_cb_t cb, void *user) {
    bool is_word = false;
    size_t offset = 0, woffset = 0;
    size_t count = 0;
    unsigned idx = 0;

    while (offset < size) {
        if (is_sep(mem[offset])) {
            if (is_word) {
                fchunk_t chunk = {
                    .offset = woffset,
                    .count = count,
                    .index = idx++,
                    .mem = mem,
                };
                is_word = false;
                cb(&chunk, user);
            }
            count = 0;
        } else {
            if (!is_word) {
                is_word = true;
                woffset = offset;
            }
            ++count;
        }
        ++offset;
    }
    if (is_word) {
        fchunk_t chunk = {
            .offset = woffset,
            .count = count,
            .index = idx,
            .mem = mem,
        };
        cb(&chunk, user);
    }
}
