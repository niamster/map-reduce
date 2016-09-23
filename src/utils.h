#ifndef _MR_UTILS_H_
#define _MR_UTILS_H_

#include <stdbool.h>
#include <sys/types.h>

void die(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
bool is_sep(char c);

typedef struct {
    unsigned index;
    off_t offset;
    size_t count;
} fchunk_t;

typedef void (*fchunk_cb_t)(int fd, fchunk_t *chunk, void *user);

int fchunk_read(int fd, unsigned max, fchunk_cb_t cb, void *user);

#endif
