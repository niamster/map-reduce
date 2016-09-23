#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"

void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    exit(1);
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
            return true;
    }

    return false;
}

int fchunk_read(int fd, unsigned max, fchunk_cb_t cb, void *user) {
    off_t csize;
    off_t offset;
    unsigned idx;

    if (fd < 0 || !cb || max == 0)
        return -EINVAL;

    csize = lseek(fd, 0, SEEK_END)/max;
    if (csize < 0)
        return -errno;

    offset = 0;
    for (idx=0; idx<max; ++idx) {
        fchunk_t chunk = {
            .offset = offset,
            .count = csize,
            .index = idx,
        };
        while (true) {
            char c;
            ssize_t res = pread(fd, &c, 1, chunk.offset + chunk.count);
            if (res < 0)
                return -errno;
            if (res == 0)
                break;
            if (is_sep(c))
                break;
            ++chunk.count;
        }
        offset += chunk.count;
        cb(fd, &chunk, user);
    }

    return 0;
}
